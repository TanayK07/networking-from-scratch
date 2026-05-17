/* Unit tests for Connection Tracking. */

#include "../conntrack.h"
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

/* ---- Init tests ---- */

static void test_init_default(void) {
    struct nfs_conntrack ct;
    ASSERT_EQ(nfs_conntrack_init(&ct, 0), 0);
    ASSERT_EQ(ct.timeout, NFS_CT_DEFAULT_TIMEOUT);
    ASSERT_EQ(ct.count, 0);
}

static void test_init_custom_timeout(void) {
    struct nfs_conntrack ct;
    ASSERT_EQ(nfs_conntrack_init(&ct, 120), 0);
    ASSERT_EQ(ct.timeout, 120);
}

static void test_init_null(void) {
    ASSERT_EQ(nfs_conntrack_init(NULL, 0), -1);
}

/* ---- TCP three-way handshake ---- */

static void test_tcp_handshake(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 300);
    time_t t = 1000;

    /* SYN -> NEW */
    struct nfs_ct_packet syn = {.src_ip = 0x0A000001,
                                .dst_ip = 0x0A000002,
                                .src_port = 12345,
                                .dst_port = 80,
                                .protocol = 6,
                                .tcp_flags = NFS_CT_TCP_SYN,
                                .length = 60};
    ASSERT_EQ(nfs_conntrack_process(&ct, &syn, t), NFS_CT_NEW);
    ASSERT_EQ(nfs_conntrack_count(&ct), 1);

    /* SYN+ACK -> ESTABLISHED */
    struct nfs_ct_packet synack = {.src_ip = 0x0A000002,
                                   .dst_ip = 0x0A000001,
                                   .src_port = 80,
                                   .dst_port = 12345,
                                   .protocol = 6,
                                   .tcp_flags = NFS_CT_TCP_SYN | NFS_CT_TCP_ACK,
                                   .length = 60};
    ASSERT_EQ(nfs_conntrack_process(&ct, &synack, t + 1), NFS_CT_ESTABLISHED);

    /* ACK -> still ESTABLISHED */
    struct nfs_ct_packet ack = {.src_ip = 0x0A000001,
                                .dst_ip = 0x0A000002,
                                .src_port = 12345,
                                .dst_port = 80,
                                .protocol = 6,
                                .tcp_flags = NFS_CT_TCP_ACK,
                                .length = 52};
    ASSERT_EQ(nfs_conntrack_process(&ct, &ack, t + 2), NFS_CT_ESTABLISHED);
    ASSERT_EQ(nfs_conntrack_count(&ct), 1);
}

/* ---- TCP RST terminates connection ---- */

static void test_tcp_rst(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 300);
    time_t t = 1000;

    /* Create connection with SYN */
    struct nfs_ct_packet syn = {.src_ip = 0x01,
                                .dst_ip = 0x02,
                                .src_port = 1000,
                                .dst_port = 80,
                                .protocol = 6,
                                .tcp_flags = NFS_CT_TCP_SYN,
                                .length = 40};
    nfs_conntrack_process(&ct, &syn, t);
    ASSERT_EQ(nfs_conntrack_count(&ct), 1);

    /* RST terminates it */
    struct nfs_ct_packet rst = {.src_ip = 0x02,
                                .dst_ip = 0x01,
                                .src_port = 80,
                                .dst_port = 1000,
                                .protocol = 6,
                                .tcp_flags = NFS_CT_TCP_RST,
                                .length = 40};
    ASSERT_EQ(nfs_conntrack_process(&ct, &rst, t + 1), NFS_CT_INVALID);
    ASSERT_EQ(nfs_conntrack_count(&ct), 0);
}

/* ---- TCP: ACK without SYN is INVALID ---- */

static void test_tcp_ack_without_syn(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 300);

    struct nfs_ct_packet ack = {.src_ip = 0x01,
                                .dst_ip = 0x02,
                                .src_port = 5000,
                                .dst_port = 443,
                                .protocol = 6,
                                .tcp_flags = NFS_CT_TCP_ACK,
                                .length = 40};
    ASSERT_EQ(nfs_conntrack_process(&ct, &ack, 1000), NFS_CT_INVALID);
    ASSERT_EQ(nfs_conntrack_count(&ct), 0);
}

/* ---- UDP connection tracking ---- */

static void test_udp_new_and_established(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 300);
    time_t t = 1000;

    /* First UDP packet -> NEW */
    struct nfs_ct_packet req = {.src_ip = 0x0A000001,
                                .dst_ip = 0x08080808,
                                .src_port = 5353,
                                .dst_port = 53,
                                .protocol = 17,
                                .tcp_flags = 0,
                                .length = 64};
    ASSERT_EQ(nfs_conntrack_process(&ct, &req, t), NFS_CT_NEW);

    /* Reply from server -> ESTABLISHED */
    struct nfs_ct_packet reply = {.src_ip = 0x08080808,
                                  .dst_ip = 0x0A000001,
                                  .src_port = 53,
                                  .dst_port = 5353,
                                  .protocol = 17,
                                  .tcp_flags = 0,
                                  .length = 128};
    ASSERT_EQ(nfs_conntrack_process(&ct, &reply, t + 1), NFS_CT_ESTABLISHED);
    ASSERT_EQ(nfs_conntrack_count(&ct), 1);
}

/* ---- Lookup tests ---- */

static void test_lookup_found(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 300);

    struct nfs_ct_packet pkt = {.src_ip = 0x0A,
                                .dst_ip = 0x0B,
                                .src_port = 100,
                                .dst_port = 200,
                                .protocol = 6,
                                .tcp_flags = NFS_CT_TCP_SYN,
                                .length = 40};
    nfs_conntrack_process(&ct, &pkt, 1000);

    struct nfs_ct_tuple t = {
        .src_ip = 0x0A, .dst_ip = 0x0B, .src_port = 100, .dst_port = 200, .protocol = 6};
    const struct nfs_ct_entry *e = nfs_conntrack_lookup(&ct, &t);
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->state, NFS_CT_NEW);
    ASSERT_EQ(e->packets, 1);
}

static void test_lookup_reverse(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 300);

    struct nfs_ct_packet pkt = {.src_ip = 0x0A,
                                .dst_ip = 0x0B,
                                .src_port = 100,
                                .dst_port = 200,
                                .protocol = 6,
                                .tcp_flags = NFS_CT_TCP_SYN,
                                .length = 40};
    nfs_conntrack_process(&ct, &pkt, 1000);

    /* Lookup in reverse direction */
    struct nfs_ct_tuple t = {
        .src_ip = 0x0B, .dst_ip = 0x0A, .src_port = 200, .dst_port = 100, .protocol = 6};
    ASSERT_TRUE(nfs_conntrack_lookup(&ct, &t) != NULL);
}

static void test_lookup_not_found(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 300);

    struct nfs_ct_tuple t = {
        .src_ip = 0x01, .dst_ip = 0x02, .src_port = 999, .dst_port = 999, .protocol = 6};
    ASSERT_TRUE(nfs_conntrack_lookup(&ct, &t) == NULL);
}

static void test_lookup_null(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 300);
    ASSERT_TRUE(nfs_conntrack_lookup(NULL, NULL) == NULL);
}

/* ---- Expiry tests ---- */

static void test_expire_idle(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 60);

    struct nfs_ct_packet pkt = {.src_ip = 0x01,
                                .dst_ip = 0x02,
                                .src_port = 100,
                                .dst_port = 200,
                                .protocol = 6,
                                .tcp_flags = NFS_CT_TCP_SYN,
                                .length = 40};
    nfs_conntrack_process(&ct, &pkt, 1000);
    ASSERT_EQ(nfs_conntrack_count(&ct), 1);

    /* Not yet expired at t=1050 */
    ASSERT_EQ(nfs_conntrack_expire(&ct, 1050), 0);
    ASSERT_EQ(nfs_conntrack_count(&ct), 1);

    /* Expired at t=1060 (60 seconds later) */
    ASSERT_EQ(nfs_conntrack_expire(&ct, 1060), 1);
    ASSERT_EQ(nfs_conntrack_count(&ct), 0);
}

static void test_expire_mixed(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 60);

    /* Entry A at t=1000 */
    struct nfs_ct_packet a = {.src_ip = 0x01,
                              .dst_ip = 0x02,
                              .src_port = 100,
                              .dst_port = 80,
                              .protocol = 6,
                              .tcp_flags = NFS_CT_TCP_SYN,
                              .length = 40};
    nfs_conntrack_process(&ct, &a, 1000);

    /* Entry B at t=1040 */
    struct nfs_ct_packet b = {.src_ip = 0x03,
                              .dst_ip = 0x04,
                              .src_port = 200,
                              .dst_port = 443,
                              .protocol = 6,
                              .tcp_flags = NFS_CT_TCP_SYN,
                              .length = 40};
    nfs_conntrack_process(&ct, &b, 1040);

    ASSERT_EQ(nfs_conntrack_count(&ct), 2);

    /* At t=1060, only A should expire */
    ASSERT_EQ(nfs_conntrack_expire(&ct, 1060), 1);
    ASSERT_EQ(nfs_conntrack_count(&ct), 1);
}

static void test_expire_null(void) {
    ASSERT_EQ(nfs_conntrack_expire(NULL, 0), -1);
}

/* ---- Packet counting ---- */

static void test_packet_counting(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 300);

    struct nfs_ct_packet pkt = {.src_ip = 0x01,
                                .dst_ip = 0x02,
                                .src_port = 100,
                                .dst_port = 80,
                                .protocol = 17,
                                .tcp_flags = 0,
                                .length = 64};
    nfs_conntrack_process(&ct, &pkt, 1000);
    nfs_conntrack_process(&ct, &pkt, 1001);
    nfs_conntrack_process(&ct, &pkt, 1002);

    struct nfs_ct_tuple t = {
        .src_ip = 0x01, .dst_ip = 0x02, .src_port = 100, .dst_port = 80, .protocol = 17};
    const struct nfs_ct_entry *e = nfs_conntrack_lookup(&ct, &t);
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->packets, 3);
    ASSERT_EQ(e->bytes, 192); /* 3 * 64 */
}

/* ---- State names ---- */

static void test_state_names(void) {
    ASSERT_TRUE(strcmp(nfs_ct_state_name(NFS_CT_NEW), "NEW") == 0);
    ASSERT_TRUE(strcmp(nfs_ct_state_name(NFS_CT_ESTABLISHED), "ESTABLISHED") == 0);
    ASSERT_TRUE(strcmp(nfs_ct_state_name(NFS_CT_RELATED), "RELATED") == 0);
    ASSERT_TRUE(strcmp(nfs_ct_state_name(NFS_CT_INVALID), "INVALID") == 0);
}

/* ---- Multiple connections ---- */

static void test_multiple_connections(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 300);

    for (int i = 0; i < 5; i++) {
        struct nfs_ct_packet pkt = {.src_ip = 0x0A000001,
                                    .dst_ip = 0x0A000002,
                                    .src_port = (uint16_t)(10000 + i),
                                    .dst_port = 80,
                                    .protocol = 6,
                                    .tcp_flags = NFS_CT_TCP_SYN,
                                    .length = 60};
        ASSERT_EQ(nfs_conntrack_process(&ct, &pkt, 1000 + i), NFS_CT_NEW);
    }
    ASSERT_EQ(nfs_conntrack_count(&ct), 5);
}

/* ---- Process null args ---- */

static void test_process_null(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 300);
    ASSERT_EQ(nfs_conntrack_process(NULL, NULL, 0), NFS_CT_INVALID);
    ASSERT_EQ(nfs_conntrack_process(&ct, NULL, 0), NFS_CT_INVALID);
}

int main(void) {
    printf("Connection Tracking Tests\n");

    test_init_default();
    test_init_custom_timeout();
    test_init_null();
    test_tcp_handshake();
    test_tcp_rst();
    test_tcp_ack_without_syn();
    test_udp_new_and_established();
    test_lookup_found();
    test_lookup_reverse();
    test_lookup_not_found();
    test_lookup_null();
    test_expire_idle();
    test_expire_mixed();
    test_expire_null();
    test_packet_counting();
    test_state_names();
    test_multiple_connections();
    test_process_null();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
