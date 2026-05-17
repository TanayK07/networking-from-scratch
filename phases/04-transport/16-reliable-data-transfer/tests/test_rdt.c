/* Unit tests for nfs_rdt — Reliable Data Transfer. */

#include "../rdt.h"
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

/* ---- sender init tests ---- */

static void test_sender_init(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 4);
    ASSERT_EQ(s.count, 0);
    ASSERT_EQ(s.base, 0);
    ASSERT_EQ(s.next_seq, 0);
    ASSERT_EQ(s.window_size, 4);
    ASSERT_EQ(s.retransmit_count, 0);
}

static void test_sender_init_stop_and_wait(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 1);
    ASSERT_EQ(s.window_size, 1);
}

/* ---- send tests ---- */

static void test_send_basic(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 4);
    uint8_t data[] = "hello";
    int seq = nfs_rdt_send(&s, data, 5);
    ASSERT_EQ(seq, 0);
    ASSERT_EQ(s.count, 1);
    ASSERT_EQ(s.next_seq, 1);
    ASSERT_EQ(s.segments[0].data_len, 5);
    ASSERT_TRUE(memcmp(s.segments[0].data, "hello", 5) == 0);
}

static void test_send_multiple(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 4);
    uint8_t d1[] = "A";
    uint8_t d2[] = "BB";
    uint8_t d3[] = "CCC";

    ASSERT_EQ(nfs_rdt_send(&s, d1, 1), 0);
    ASSERT_EQ(nfs_rdt_send(&s, d2, 2), 1);
    ASSERT_EQ(nfs_rdt_send(&s, d3, 3), 2);
    ASSERT_EQ(s.count, 3);
}

static void test_send_window_full(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 2);
    uint8_t d[] = "x";
    ASSERT_EQ(nfs_rdt_send(&s, d, 1), 0);
    ASSERT_EQ(nfs_rdt_send(&s, d, 1), 1);
    /* Window full, should reject */
    ASSERT_EQ(nfs_rdt_send(&s, d, 1), -1);
}

static void test_send_stop_and_wait_window_1(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 1);
    uint8_t d[] = "x";
    ASSERT_EQ(nfs_rdt_send(&s, d, 1), 0);
    /* Window of 1, so second send should fail */
    ASSERT_EQ(nfs_rdt_send(&s, d, 1), -1);
}

static void test_send_after_ack(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 1);
    uint8_t d[] = "x";
    ASSERT_EQ(nfs_rdt_send(&s, d, 1), 0);
    nfs_rdt_ack(&s, 1); /* ACK seq 0 */
    /* Window should open */
    ASSERT_EQ(nfs_rdt_send(&s, d, 1), 1);
}

/* ---- ACK tests ---- */

static void test_ack_advances_base(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 4);
    uint8_t d[] = "x";
    nfs_rdt_send(&s, d, 1);
    nfs_rdt_send(&s, d, 1);
    nfs_rdt_send(&s, d, 1);

    ASSERT_EQ(s.base, 0);
    int acked = nfs_rdt_ack(&s, 2); /* ACK seqs 0 and 1 */
    ASSERT_EQ(acked, 2);
    ASSERT_EQ(s.base, 2);
}

static void test_ack_cumulative(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 4);
    uint8_t d[] = "x";
    for (int i = 0; i < 4; i++)
        nfs_rdt_send(&s, d, 1);

    /* ACK all at once */
    int acked = nfs_rdt_ack(&s, 4);
    ASSERT_EQ(acked, 4);
    ASSERT_EQ(s.base, 4);
    ASSERT_EQ(nfs_rdt_in_flight(&s), 0);
}

static void test_ack_no_effect_if_old(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 4);
    uint8_t d[] = "x";
    nfs_rdt_send(&s, d, 1);
    nfs_rdt_send(&s, d, 1);
    nfs_rdt_ack(&s, 2);

    /* Duplicate/old ACK */
    int acked = nfs_rdt_ack(&s, 1);
    ASSERT_EQ(acked, 0);
    ASSERT_EQ(s.base, 2);
}

/* ---- timeout tests ---- */

static void test_timeout_retransmits_window(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 4);
    uint8_t d[] = "x";
    for (int i = 0; i < 4; i++)
        nfs_rdt_send(&s, d, 1);

    int retx = nfs_rdt_timeout(&s);
    ASSERT_EQ(retx, 4);
    ASSERT_EQ(s.retransmit_count, 4);
}

static void test_timeout_only_unacked(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 4);
    uint8_t d[] = "x";
    for (int i = 0; i < 4; i++)
        nfs_rdt_send(&s, d, 1);

    nfs_rdt_ack(&s, 2); /* ACK first 2 */
    int retx = nfs_rdt_timeout(&s);
    ASSERT_EQ(retx, 2);
}

static void test_timeout_stop_and_wait(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 1);
    uint8_t d[] = "x";
    nfs_rdt_send(&s, d, 1);
    int retx = nfs_rdt_timeout(&s);
    ASSERT_EQ(retx, 1);
}

static void test_timeout_increments_retransmit_count(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 2);
    uint8_t d[] = "x";
    nfs_rdt_send(&s, d, 1);
    nfs_rdt_send(&s, d, 1);

    nfs_rdt_timeout(&s);
    ASSERT_EQ(s.retransmit_count, 2);
    nfs_rdt_timeout(&s);
    ASSERT_EQ(s.retransmit_count, 4);
}

/* ---- in_flight tests ---- */

static void test_in_flight(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 4);
    ASSERT_EQ(nfs_rdt_in_flight(&s), 0);

    uint8_t d[] = "x";
    nfs_rdt_send(&s, d, 1);
    ASSERT_EQ(nfs_rdt_in_flight(&s), 1);

    nfs_rdt_send(&s, d, 1);
    nfs_rdt_send(&s, d, 1);
    ASSERT_EQ(nfs_rdt_in_flight(&s), 3);

    nfs_rdt_ack(&s, 2);
    ASSERT_EQ(nfs_rdt_in_flight(&s), 1);

    nfs_rdt_ack(&s, 3);
    ASSERT_EQ(nfs_rdt_in_flight(&s), 0);
}

/* ---- receiver tests ---- */

static void test_receiver_init(void) {
    struct nfs_rdt_receiver r;
    nfs_rdt_receiver_init(&r);
    ASSERT_EQ(r.expected_seq, 0);
    ASSERT_EQ(r.recv_len, 0);
}

static void test_receive_in_order(void) {
    struct nfs_rdt_receiver r;
    nfs_rdt_receiver_init(&r);

    struct nfs_rdt_segment seg;
    seg.seq = 0;
    memcpy(seg.data, "hello", 5);
    seg.data_len = 5;

    uint32_t ack;
    int rc = nfs_rdt_receive(&r, &seg, &ack);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(ack, 1);
    ASSERT_EQ(r.expected_seq, 1);
    ASSERT_EQ(r.recv_len, 5);
    ASSERT_TRUE(memcmp(r.recv_buf, "hello", 5) == 0);
}

static void test_receive_out_of_order(void) {
    struct nfs_rdt_receiver r;
    nfs_rdt_receiver_init(&r);

    struct nfs_rdt_segment seg;
    seg.seq = 1; /* expected is 0 */
    memcpy(seg.data, "world", 5);
    seg.data_len = 5;

    uint32_t ack;
    int rc = nfs_rdt_receive(&r, &seg, &ack);
    ASSERT_EQ(rc, -1);
    ASSERT_EQ(ack, 0); /* duplicate ACK for expected */
    ASSERT_EQ(r.recv_len, 0);
}

static void test_receive_duplicate_ack(void) {
    struct nfs_rdt_receiver r;
    nfs_rdt_receiver_init(&r);

    /* Deliver seq 0 */
    struct nfs_rdt_segment seg0 = {.seq = 0, .data_len = 3};
    memcpy(seg0.data, "aaa", 3);
    uint32_t ack;
    nfs_rdt_receive(&r, &seg0, &ack);
    ASSERT_EQ(ack, 1);

    /* Send seq 5 (out of order) — should get dup ACK for seq 1 */
    struct nfs_rdt_segment seg5 = {.seq = 5, .data_len = 3};
    memcpy(seg5.data, "bbb", 3);
    int rc = nfs_rdt_receive(&r, &seg5, &ack);
    ASSERT_EQ(rc, -1);
    ASSERT_EQ(ack, 1);
}

static void test_receive_sequential(void) {
    struct nfs_rdt_receiver r;
    nfs_rdt_receiver_init(&r);

    for (uint32_t i = 0; i < 5; i++) {
        struct nfs_rdt_segment seg;
        seg.seq = i;
        seg.data[0] = (uint8_t)('A' + i);
        seg.data_len = 1;

        uint32_t ack;
        int rc = nfs_rdt_receive(&r, &seg, &ack);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(ack, i + 1);
    }
    ASSERT_EQ(r.recv_len, 5);
    ASSERT_TRUE(memcmp(r.recv_buf, "ABCDE", 5) == 0);
}

/* ---- end-to-end scenario: stop-and-wait ---- */

static void test_stop_and_wait_scenario(void) {
    struct nfs_rdt_sender s;
    struct nfs_rdt_receiver r;
    nfs_rdt_sender_init(&s, 1);
    nfs_rdt_receiver_init(&r);

    const char *msgs[] = {"one", "two", "end"};
    for (int i = 0; i < 3; i++) {
        int seq = nfs_rdt_send(&s, (const uint8_t *)msgs[i], strlen(msgs[i]));
        ASSERT_TRUE(seq >= 0);

        uint32_t ack;
        int rc = nfs_rdt_receive(&r, &s.segments[s.count - 1], &ack);
        ASSERT_EQ(rc, 0);

        int acked = nfs_rdt_ack(&s, ack);
        ASSERT_EQ(acked, 1);
    }
    ASSERT_EQ(r.recv_len, 9);
    ASSERT_EQ(nfs_rdt_in_flight(&s), 0);
}

/* ---- end-to-end scenario: Go-Back-N with loss ---- */

static void test_gbn_with_loss(void) {
    struct nfs_rdt_sender s;
    struct nfs_rdt_receiver r;
    nfs_rdt_sender_init(&s, 4);
    nfs_rdt_receiver_init(&r);

    /* Send 4 segments */
    for (int i = 0; i < 4; i++) {
        uint8_t d = (uint8_t)('A' + i);
        nfs_rdt_send(&s, &d, 1);
    }
    ASSERT_EQ(nfs_rdt_in_flight(&s), 4);

    /* Deliver only seg 0 (simulate loss of 1, 2, 3) */
    uint32_t ack;
    nfs_rdt_receive(&r, &s.segments[0], &ack);
    nfs_rdt_ack(&s, ack);
    ASSERT_EQ(nfs_rdt_in_flight(&s), 3);

    /* Timeout: retransmit unacked in window */
    int retx = nfs_rdt_timeout(&s);
    ASSERT_EQ(retx, 3);

    /* Now deliver remaining */
    for (uint32_t i = s.base; i < s.count; i++) {
        nfs_rdt_receive(&r, &s.segments[i], &ack);
        nfs_rdt_ack(&s, ack);
    }
    ASSERT_EQ(nfs_rdt_in_flight(&s), 0);
    ASSERT_EQ(r.recv_len, 4);
    ASSERT_TRUE(memcmp(r.recv_buf, "ABCD", 4) == 0);
}

/* ---- sender free/reset ---- */

static void test_sender_free(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 4);
    uint8_t d[] = "x";
    nfs_rdt_send(&s, d, 1);
    nfs_rdt_sender_free(&s);
    ASSERT_EQ(s.count, 0);
    ASSERT_EQ(s.base, 0);
    ASSERT_EQ(s.next_seq, 0);
}

/* ---- send rejects ---- */

static void test_send_null_data(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 4);
    ASSERT_EQ(nfs_rdt_send(&s, NULL, 5), -1);
}

static void test_send_zero_len(void) {
    struct nfs_rdt_sender s;
    nfs_rdt_sender_init(&s, 4);
    uint8_t d[] = "x";
    ASSERT_EQ(nfs_rdt_send(&s, d, 0), -1);
}

int main(void) {
    printf("Reliable Data Transfer Tests\n");

    test_sender_init();
    test_sender_init_stop_and_wait();
    test_send_basic();
    test_send_multiple();
    test_send_window_full();
    test_send_stop_and_wait_window_1();
    test_send_after_ack();
    test_ack_advances_base();
    test_ack_cumulative();
    test_ack_no_effect_if_old();
    test_timeout_retransmits_window();
    test_timeout_only_unacked();
    test_timeout_stop_and_wait();
    test_timeout_increments_retransmit_count();
    test_in_flight();
    test_receiver_init();
    test_receive_in_order();
    test_receive_out_of_order();
    test_receive_duplicate_ack();
    test_receive_sequential();
    test_stop_and_wait_scenario();
    test_gbn_with_loss();
    test_sender_free();
    test_send_null_data();
    test_send_zero_len();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
