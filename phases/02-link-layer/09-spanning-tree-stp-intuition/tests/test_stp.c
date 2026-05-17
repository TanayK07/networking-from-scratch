/* Unit tests for STP (IEEE 802.1D) BPDU handling and bridge election. */

#include "../stp.h"

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

/* ------------------------------------------------------------------ */
/*  Helper: make a bridge ID                                           */
/* ------------------------------------------------------------------ */
static struct nfs_bridge_id make_bid(uint16_t prio, uint8_t m0, uint8_t m1, uint8_t m2, uint8_t m3,
                                     uint8_t m4, uint8_t m5) {
    struct nfs_bridge_id id;
    id.priority = prio;
    id.mac[0] = m0;
    id.mac[1] = m1;
    id.mac[2] = m2;
    id.mac[3] = m3;
    id.mac[4] = m4;
    id.mac[5] = m5;
    return id;
}

/* ------------------------------------------------------------------ */
/*  1. Bridge ID compare: priority wins                                */
/* ------------------------------------------------------------------ */
static void test_bridge_id_cmp_priority(void) {
    struct nfs_bridge_id a = make_bid(4096, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);
    struct nfs_bridge_id b = make_bid(32768, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    ASSERT_TRUE(nfs_bridge_id_cmp(&a, &b) < 0);
}

/* ------------------------------------------------------------------ */
/*  2. Bridge ID compare: MAC tiebreak                                 */
/* ------------------------------------------------------------------ */
static void test_bridge_id_cmp_mac(void) {
    struct nfs_bridge_id a = make_bid(32768, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    struct nfs_bridge_id b = make_bid(32768, 0x00, 0x11, 0x22, 0x33, 0x44, 0x66);
    ASSERT_TRUE(nfs_bridge_id_cmp(&a, &b) < 0);
}

/* ------------------------------------------------------------------ */
/*  3. Bridge ID compare: equal                                        */
/* ------------------------------------------------------------------ */
static void test_bridge_id_cmp_equal(void) {
    struct nfs_bridge_id a = make_bid(32768, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    struct nfs_bridge_id b = make_bid(32768, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    ASSERT_EQ(nfs_bridge_id_cmp(&a, &b), 0);
}

/* ------------------------------------------------------------------ */
/*  4. Bridge ID pack/unpack roundtrip                                 */
/* ------------------------------------------------------------------ */
static void test_bridge_id_roundtrip(void) {
    struct nfs_bridge_id orig = make_bid(32768, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);
    uint8_t packed[8];
    nfs_bridge_id_pack(&orig, packed);

    struct nfs_bridge_id unpacked;
    nfs_bridge_id_unpack(packed, &unpacked);

    ASSERT_EQ(unpacked.priority, 32768);
    ASSERT_EQ(unpacked.mac[0], 0xAA);
    ASSERT_EQ(unpacked.mac[1], 0xBB);
    ASSERT_EQ(unpacked.mac[2], 0xCC);
    ASSERT_EQ(unpacked.mac[3], 0xDD);
    ASSERT_EQ(unpacked.mac[4], 0xEE);
    ASSERT_EQ(unpacked.mac[5], 0xFF);
}

/* ------------------------------------------------------------------ */
/*  5. Bridge ID pack network byte order                               */
/* ------------------------------------------------------------------ */
static void test_bridge_id_pack_nbo(void) {
    struct nfs_bridge_id id = make_bid(0x8000, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06);
    uint8_t packed[8];
    nfs_bridge_id_pack(&id, packed);

    /* Priority 0x8000 in network byte order = 0x80, 0x00 */
    ASSERT_EQ(packed[0], 0x80);
    ASSERT_EQ(packed[1], 0x00);
    /* MAC follows directly */
    ASSERT_EQ(packed[2], 0x01);
    ASSERT_EQ(packed[7], 0x06);
}

/* ------------------------------------------------------------------ */
/*  6. BPDU build config: 35 bytes                                     */
/* ------------------------------------------------------------------ */
static void test_bpdu_build_config(void) {
    struct nfs_bpdu bpdu;
    memset(&bpdu, 0, sizeof(bpdu));
    bpdu.proto_id = 0;
    bpdu.version = 0;
    bpdu.type = 0x00; /* Configuration */
    bpdu.root_id = make_bid(32768, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    bpdu.root_path_cost = 0;
    bpdu.sender_id = make_bid(32768, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    bpdu.port_id = 1;
    bpdu.message_age = 0;
    bpdu.max_age = 5120;
    bpdu.hello_time = 512;
    bpdu.forward_delay = 3840;

    uint8_t buf[64];
    int n = nfs_bpdu_build(&bpdu, buf, sizeof(buf));
    ASSERT_EQ(n, 35);

    /* proto_id should be 0x0000 in network order */
    ASSERT_EQ(buf[0], 0x00);
    ASSERT_EQ(buf[1], 0x00);

    /* version = 0, type = 0x00 */
    ASSERT_EQ(buf[2], 0x00);
    ASSERT_EQ(buf[3], 0x00);
}

/* ------------------------------------------------------------------ */
/*  7. BPDU build TCN: 4 bytes                                        */
/* ------------------------------------------------------------------ */
static void test_bpdu_build_tcn(void) {
    struct nfs_bpdu bpdu;
    memset(&bpdu, 0, sizeof(bpdu));
    bpdu.proto_id = 0;
    bpdu.version = 0;
    bpdu.type = 0x80; /* TCN */

    uint8_t buf[64];
    int n = nfs_bpdu_build(&bpdu, buf, sizeof(buf));
    ASSERT_EQ(n, 4);
    ASSERT_EQ(buf[3], 0x80);
}

/* ------------------------------------------------------------------ */
/*  8. BPDU parse roundtrip                                            */
/* ------------------------------------------------------------------ */
static void test_bpdu_parse_roundtrip(void) {
    struct nfs_bpdu orig;
    memset(&orig, 0, sizeof(orig));
    orig.proto_id = 0;
    orig.version = 0;
    orig.type = 0x00;
    orig.flags = 0x01; /* TC bit set */
    orig.root_id = make_bid(4096, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF);
    orig.root_path_cost = 42;
    orig.sender_id = make_bid(32768, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66);
    orig.port_id = 0x8001;
    orig.message_age = 256;
    orig.max_age = 5120;
    orig.hello_time = 512;
    orig.forward_delay = 3840;

    uint8_t buf[64];
    int n = nfs_bpdu_build(&orig, buf, sizeof(buf));
    ASSERT_EQ(n, 35);

    struct nfs_bpdu parsed;
    ASSERT_EQ(nfs_bpdu_parse(buf, (size_t)n, &parsed), 0);

    ASSERT_EQ(parsed.proto_id, 0);
    ASSERT_EQ(parsed.version, 0);
    ASSERT_EQ(parsed.type, 0x00);
    ASSERT_EQ(parsed.flags, 0x01);
    ASSERT_EQ(parsed.root_id.priority, 4096);
    ASSERT_EQ(parsed.root_id.mac[0], 0xAA);
    ASSERT_EQ(parsed.root_path_cost, 42);
    ASSERT_EQ(parsed.sender_id.priority, 32768);
    ASSERT_EQ(parsed.sender_id.mac[0], 0x11);
    ASSERT_EQ(parsed.port_id, 0x8001);
    ASSERT_EQ(parsed.message_age, 256);
    ASSERT_EQ(parsed.max_age, 5120);
    ASSERT_EQ(parsed.hello_time, 512);
    ASSERT_EQ(parsed.forward_delay, 3840);
}

/* ------------------------------------------------------------------ */
/*  9. BPDU parse rejects short                                        */
/* ------------------------------------------------------------------ */
static void test_bpdu_parse_short(void) {
    uint8_t buf[3] = {0x00, 0x00, 0x00};
    struct nfs_bpdu bpdu;
    ASSERT_EQ(nfs_bpdu_parse(buf, 3, &bpdu), -1);
}

/* ------------------------------------------------------------------ */
/*  10. BPDU parse rejects bad proto                                   */
/* ------------------------------------------------------------------ */
static void test_bpdu_parse_bad_proto(void) {
    uint8_t buf[35];
    memset(buf, 0, sizeof(buf));
    /* Set proto_id to 0x0001 (invalid) in network byte order */
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = 0x00; /* version */
    buf[3] = 0x00; /* type = config */

    struct nfs_bpdu bpdu;
    ASSERT_EQ(nfs_bpdu_parse(buf, sizeof(buf), &bpdu), -1);
}

/* ------------------------------------------------------------------ */
/*  11. BPDU superiority: lower root wins                              */
/* ------------------------------------------------------------------ */
static void test_bpdu_superior_root(void) {
    struct nfs_bpdu a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));

    a.root_id = make_bid(4096, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01);
    b.root_id = make_bid(32768, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01);

    ASSERT_EQ(nfs_bpdu_superior(&a, &b), 1);
    ASSERT_EQ(nfs_bpdu_superior(&b, &a), 0);
}

/* ------------------------------------------------------------------ */
/*  12. BPDU superiority: same root, lower cost wins                   */
/* ------------------------------------------------------------------ */
static void test_bpdu_superior_cost(void) {
    struct nfs_bpdu a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));

    struct nfs_bridge_id root = make_bid(32768, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01);
    a.root_id = root;
    b.root_id = root;
    a.root_path_cost = 10;
    b.root_path_cost = 20;

    ASSERT_EQ(nfs_bpdu_superior(&a, &b), 1);
    ASSERT_EQ(nfs_bpdu_superior(&b, &a), 0);
}

/* ------------------------------------------------------------------ */
/*  13. BPDU superiority: full tiebreak (sender_id differs)            */
/* ------------------------------------------------------------------ */
static void test_bpdu_superior_sender(void) {
    struct nfs_bpdu a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));

    struct nfs_bridge_id root = make_bid(32768, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01);
    a.root_id = root;
    b.root_id = root;
    a.root_path_cost = 10;
    b.root_path_cost = 10;
    a.sender_id = make_bid(32768, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02);
    b.sender_id = make_bid(32768, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03);

    ASSERT_EQ(nfs_bpdu_superior(&a, &b), 1);
    ASSERT_EQ(nfs_bpdu_superior(&b, &a), 0);
}

/* ------------------------------------------------------------------ */
/*  14. Port init: starts in BLOCKING state                            */
/* ------------------------------------------------------------------ */
static void test_port_init(void) {
    struct nfs_stp_port port;
    nfs_stp_port_init(&port, 0x8001, 19);
    ASSERT_EQ(port.state, NFS_STP_BLOCKING);
    ASSERT_EQ(port.port_id, 0x8001);
    ASSERT_EQ(port.path_cost, 19);
}

/* ------------------------------------------------------------------ */
/*  15. Path cost 10Mbps -> 100                                        */
/* ------------------------------------------------------------------ */
static void test_path_cost_10(void) {
    ASSERT_EQ(nfs_stp_path_cost(10), 100);
}

/* ------------------------------------------------------------------ */
/*  16. Path cost 100Mbps -> 19                                        */
/* ------------------------------------------------------------------ */
static void test_path_cost_100(void) {
    ASSERT_EQ(nfs_stp_path_cost(100), 19);
}

/* ------------------------------------------------------------------ */
/*  17. Path cost 1Gbps -> 4                                           */
/* ------------------------------------------------------------------ */
static void test_path_cost_1000(void) {
    ASSERT_EQ(nfs_stp_path_cost(1000), 4);
}

/* ------------------------------------------------------------------ */
/*  18. Path cost 10Gbps -> 2                                          */
/* ------------------------------------------------------------------ */
static void test_path_cost_10000(void) {
    ASSERT_EQ(nfs_stp_path_cost(10000), 2);
}

/* ------------------------------------------------------------------ */
/*  19. Bridge init: self-root                                         */
/* ------------------------------------------------------------------ */
static void test_bridge_init_self_root(void) {
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    struct nfs_stp_bridge br;
    nfs_stp_bridge_init(&br, NFS_STP_DEFAULT_PRIORITY, mac);

    ASSERT_EQ(br.id.priority, NFS_STP_DEFAULT_PRIORITY);
    ASSERT_EQ(nfs_bridge_id_cmp(&br.root_id, &br.id), 0);
    ASSERT_EQ(br.root_path_cost, 0);
    ASSERT_EQ(br.root_port, -1);
}

/* ------------------------------------------------------------------ */
/*  20. Process superior BPDU: bridge updates root_id and root_port    */
/* ------------------------------------------------------------------ */
static void test_process_superior_bpdu(void) {
    /* Bridge B has higher MAC */
    uint8_t mac_b[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x99};
    struct nfs_stp_bridge br;
    nfs_stp_bridge_init(&br, NFS_STP_DEFAULT_PRIORITY, mac_b);
    br.num_ports = 2;
    nfs_stp_port_init(&br.ports[0], 1, 19);
    nfs_stp_port_init(&br.ports[1], 2, 19);

    /* Receive a BPDU from a bridge advertising a better root (lower MAC) */
    struct nfs_bpdu bpdu;
    memset(&bpdu, 0, sizeof(bpdu));
    bpdu.root_id = make_bid(NFS_STP_DEFAULT_PRIORITY, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01);
    bpdu.root_path_cost = 0;
    bpdu.sender_id = make_bid(NFS_STP_DEFAULT_PRIORITY, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02);
    bpdu.port_id = 1;

    nfs_stp_process_bpdu(&br, 0, &bpdu);

    /* Bridge should now know about the better root */
    ASSERT_EQ(br.root_id.mac[5], 0x01);
    ASSERT_EQ(br.root_port, 0);
    ASSERT_EQ(br.root_path_cost, 19); /* 0 + port path cost */
    ASSERT_EQ(br.ports[0].role, NFS_STP_ROOT_PORT);
}

/* ------------------------------------------------------------------ */
/*  21. Process inferior BPDU: bridge ignores, stays root              */
/* ------------------------------------------------------------------ */
static void test_process_inferior_bpdu(void) {
    /* Bridge A has the lowest MAC — it should stay root */
    uint8_t mac_a[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    struct nfs_stp_bridge br;
    nfs_stp_bridge_init(&br, NFS_STP_DEFAULT_PRIORITY, mac_a);
    br.num_ports = 2;
    nfs_stp_port_init(&br.ports[0], 1, 19);
    nfs_stp_port_init(&br.ports[1], 2, 19);

    /* Receive a BPDU advertising a worse root (higher MAC) */
    struct nfs_bpdu bpdu;
    memset(&bpdu, 0, sizeof(bpdu));
    bpdu.root_id = make_bid(NFS_STP_DEFAULT_PRIORITY, 0x00, 0x00, 0x00, 0x00, 0x00, 0x99);
    bpdu.root_path_cost = 0;
    bpdu.sender_id = make_bid(NFS_STP_DEFAULT_PRIORITY, 0x00, 0x00, 0x00, 0x00, 0x00, 0x99);
    bpdu.port_id = 1;

    nfs_stp_process_bpdu(&br, 0, &bpdu);

    /* Bridge should still be root */
    ASSERT_EQ(br.root_id.mac[5], 0x01);
    ASSERT_EQ(br.root_port, -1);
    ASSERT_EQ(br.root_path_cost, 0);
}

/* ------------------------------------------------------------------ */
/*  22. STP defaults: hello=2, max_age=20, fwd_delay=15               */
/* ------------------------------------------------------------------ */
static void test_stp_defaults(void) {
    ASSERT_EQ(NFS_STP_HELLO_TIME, 2);
    ASSERT_EQ(NFS_STP_MAX_AGE, 20);
    ASSERT_EQ(NFS_STP_FWD_DELAY, 15);
}

/* ------------------------------------------------------------------ */
/*  23. BPDU timer encoding: 2 seconds = 512 in 1/256 format          */
/* ------------------------------------------------------------------ */
static void test_bpdu_timer_encoding(void) {
    /* 2 seconds * 256 = 512 */
    ASSERT_EQ(NFS_STP_HELLO_TIME * 256, 512);
    /* 20 seconds * 256 = 5120 */
    ASSERT_EQ(NFS_STP_MAX_AGE * 256, 5120);
    /* 15 seconds * 256 = 3840 */
    ASSERT_EQ(NFS_STP_FWD_DELAY * 256, 3840);
}

/* ------------------------------------------------------------------ */
/*  24. Port state constants: BLOCKING=1, FORWARDING=4                 */
/* ------------------------------------------------------------------ */
static void test_port_state_constants(void) {
    ASSERT_EQ(NFS_STP_DISABLED, 0);
    ASSERT_EQ(NFS_STP_BLOCKING, 1);
    ASSERT_EQ(NFS_STP_LISTENING, 2);
    ASSERT_EQ(NFS_STP_LEARNING, 3);
    ASSERT_EQ(NFS_STP_FORWARDING, 4);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_bridge_id_cmp_priority();
    test_bridge_id_cmp_mac();
    test_bridge_id_cmp_equal();
    test_bridge_id_roundtrip();
    test_bridge_id_pack_nbo();
    test_bpdu_build_config();
    test_bpdu_build_tcn();
    test_bpdu_parse_roundtrip();
    test_bpdu_parse_short();
    test_bpdu_parse_bad_proto();
    test_bpdu_superior_root();
    test_bpdu_superior_cost();
    test_bpdu_superior_sender();
    test_port_init();
    test_path_cost_10();
    test_path_cost_100();
    test_path_cost_1000();
    test_path_cost_10000();
    test_bridge_init_self_root();
    test_process_superior_bpdu();
    test_process_inferior_bpdu();
    test_stp_defaults();
    test_bpdu_timer_encoding();
    test_port_state_constants();

    printf("\n  %d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        fprintf(stderr, "  SOME TESTS FAILED\n");
        return 1;
    }
    printf("  All tests passed.\n");
    return 0;
}
