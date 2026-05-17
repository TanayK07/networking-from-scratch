/* Unit tests for Nagle's algorithm and delayed ACK. */

#include "../nagle.h"
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

/* ---- Nagle: full MSS always sends ---- */

static void test_nagle_full_mss_sends(void) {
    struct nfs_nagle n;
    nfs_nagle_init(&n, 1460);

    /* Even with unacked data, full MSS goes out */
    nfs_nagle_sent(&n, 100); /* create unacked data */
    ASSERT_EQ(nfs_nagle_can_send(&n, 1460), 1);
    ASSERT_EQ(nfs_nagle_can_send(&n, 1461), 1); /* larger than MSS */
    ASSERT_EQ(nfs_nagle_can_send(&n, 2920), 1); /* 2x MSS */
}

/* ---- Nagle: blocks small data when unacked ---- */

static void test_nagle_blocks_small_when_unacked(void) {
    struct nfs_nagle n;
    nfs_nagle_init(&n, 1460);

    /* Send some data to create outstanding bytes */
    nfs_nagle_sent(&n, 50);
    ASSERT_EQ(n.snd_nxt, 50);
    ASSERT_EQ(n.snd_una, 0);

    /* Small data should be blocked */
    ASSERT_EQ(nfs_nagle_can_send(&n, 1), 0);
    ASSERT_EQ(nfs_nagle_can_send(&n, 10), 0);
    ASSERT_EQ(nfs_nagle_can_send(&n, 100), 0);
    ASSERT_EQ(nfs_nagle_can_send(&n, 1459), 0);
}

/* ---- Nagle: allows when no unacked data ---- */

static void test_nagle_allows_no_unacked(void) {
    struct nfs_nagle n;
    nfs_nagle_init(&n, 1460);

    /* Initially snd_una == snd_nxt == 0 */
    ASSERT_EQ(nfs_nagle_can_send(&n, 1), 1);
    ASSERT_EQ(nfs_nagle_can_send(&n, 10), 1);

    /* Send and then ACK */
    nfs_nagle_sent(&n, 100);
    nfs_nagle_acked(&n, 100);
    ASSERT_EQ(n.snd_una, 100);
    ASSERT_EQ(n.snd_nxt, 100);
    ASSERT_EQ(nfs_nagle_can_send(&n, 5), 1);
}

/* ---- Nagle disabled always sends ---- */

static void test_nagle_disabled_always_sends(void) {
    struct nfs_nagle n;
    nfs_nagle_init(&n, 1460);
    nfs_nagle_disable(&n);

    /* Create unacked data */
    nfs_nagle_sent(&n, 200);
    ASSERT_EQ(n.snd_una, 0);
    ASSERT_EQ(n.snd_nxt, 200);

    /* Small data should still go out */
    ASSERT_EQ(nfs_nagle_can_send(&n, 1), 1);
    ASSERT_EQ(nfs_nagle_can_send(&n, 10), 1);
    ASSERT_EQ(nfs_nagle_can_send(&n, 1459), 1);
}

/* ---- Nagle enable/disable toggle ---- */

static void test_nagle_toggle(void) {
    struct nfs_nagle n;
    nfs_nagle_init(&n, 1460);
    ASSERT_EQ(n.enabled, 1);

    nfs_nagle_disable(&n);
    ASSERT_EQ(n.enabled, 0);

    nfs_nagle_enable(&n);
    ASSERT_EQ(n.enabled, 1);
}

/* ---- Nagle: ACK unblocks buffered data ---- */

static void test_nagle_ack_unblocks(void) {
    struct nfs_nagle n;
    nfs_nagle_init(&n, 1460);

    nfs_nagle_sent(&n, 100);
    ASSERT_EQ(nfs_nagle_can_send(&n, 5), 0); /* blocked */

    /* Partial ACK: still blocked */
    nfs_nagle_acked(&n, 50);
    ASSERT_EQ(nfs_nagle_can_send(&n, 5), 0);

    /* Full ACK: unblocked */
    nfs_nagle_acked(&n, 100);
    ASSERT_EQ(nfs_nagle_can_send(&n, 5), 1);
}

/* ---- Nagle: MSS boundary ---- */

static void test_nagle_mss_boundary(void) {
    struct nfs_nagle n;
    nfs_nagle_init(&n, 536); /* minimum MSS per RFC 879 */

    nfs_nagle_sent(&n, 100);                   /* unacked data */
    ASSERT_EQ(nfs_nagle_can_send(&n, 535), 0); /* just below MSS */
    ASSERT_EQ(nfs_nagle_can_send(&n, 536), 1); /* exactly MSS */
    ASSERT_EQ(nfs_nagle_can_send(&n, 537), 1); /* above MSS */
}

/* ---- Delayed ACK: waits 200ms ---- */

static void test_delayed_ack_waits(void) {
    struct nfs_delayed_ack da;
    nfs_delayed_ack_init(&da, 0.200);

    double now = 1.000;
    int ack_now = nfs_delayed_ack_receive(&da, now);
    ASSERT_EQ(ack_now, 0); /* first segment: delay */
    ASSERT_EQ(da.pending, 1);
    ASSERT_TRUE(da.deadline > 1.199 && da.deadline < 1.201);

    /* Before deadline */
    ASSERT_EQ(nfs_delayed_ack_check(&da, 1.100), 0);
    ASSERT_EQ(nfs_delayed_ack_check(&da, 1.199), 0);

    /* At deadline */
    ASSERT_EQ(nfs_delayed_ack_check(&da, 1.200), 1);

    /* After deadline */
    ASSERT_EQ(nfs_delayed_ack_check(&da, 1.300), 1);
}

/* ---- Delayed ACK: 2nd segment triggers immediate ACK ---- */

static void test_delayed_ack_second_segment(void) {
    struct nfs_delayed_ack da;
    nfs_delayed_ack_init(&da, 0.200);

    double now = 5.000;
    ASSERT_EQ(nfs_delayed_ack_receive(&da, now), 0); /* seg 1: delay */

    now = 5.050;
    ASSERT_EQ(nfs_delayed_ack_receive(&da, now), 1); /* seg 2: immediate */
}

/* ---- Delayed ACK: sent resets state ---- */

static void test_delayed_ack_sent_resets(void) {
    struct nfs_delayed_ack da;
    nfs_delayed_ack_init(&da, 0.200);

    nfs_delayed_ack_receive(&da, 1.0);
    ASSERT_EQ(da.pending, 1);
    ASSERT_EQ(da.seg_count, 1);

    nfs_delayed_ack_sent(&da);
    ASSERT_EQ(da.pending, 0);
    ASSERT_EQ(da.seg_count, 0);

    /* After reset, first segment again delays */
    ASSERT_EQ(nfs_delayed_ack_receive(&da, 2.0), 0);
    ASSERT_EQ(da.seg_count, 1);
}

/* ---- Delayed ACK: check on non-pending returns 0 ---- */

static void test_delayed_ack_check_not_pending(void) {
    struct nfs_delayed_ack da;
    nfs_delayed_ack_init(&da, 0.200);

    /* No data received yet */
    ASSERT_EQ(nfs_delayed_ack_check(&da, 999.0), 0);
}

/* ---- Delayed ACK: custom delay ---- */

static void test_delayed_ack_custom_delay(void) {
    struct nfs_delayed_ack da;
    nfs_delayed_ack_init(&da, 0.040); /* 40ms delay */

    nfs_delayed_ack_receive(&da, 0.0);
    ASSERT_EQ(nfs_delayed_ack_check(&da, 0.039), 0);
    ASSERT_EQ(nfs_delayed_ack_check(&da, 0.040), 1);
}

/* ---- Delayed ACK: full cycle ---- */

static void test_delayed_ack_full_cycle(void) {
    struct nfs_delayed_ack da;
    nfs_delayed_ack_init(&da, 0.200);

    /* Cycle 1: single segment, wait for timeout */
    ASSERT_EQ(nfs_delayed_ack_receive(&da, 0.0), 0);
    ASSERT_EQ(nfs_delayed_ack_check(&da, 0.200), 1);
    nfs_delayed_ack_sent(&da);

    /* Cycle 2: two segments, immediate ACK */
    ASSERT_EQ(nfs_delayed_ack_receive(&da, 1.0), 0);
    ASSERT_EQ(nfs_delayed_ack_receive(&da, 1.1), 1);
    nfs_delayed_ack_sent(&da);

    /* Cycle 3: verify clean state */
    ASSERT_EQ(da.pending, 0);
    ASSERT_EQ(da.seg_count, 0);
}

/* ---- Nagle: initial state ---- */

static void test_nagle_initial_state(void) {
    struct nfs_nagle n;
    nfs_nagle_init(&n, 1460);

    ASSERT_EQ(n.enabled, 1);
    ASSERT_EQ(n.snd_una, 0);
    ASSERT_EQ(n.snd_nxt, 0);
    ASSERT_EQ(n.mss, 1460);
}

/* ---- Nagle: sent advances snd_nxt ---- */

static void test_nagle_sent_advances(void) {
    struct nfs_nagle n;
    nfs_nagle_init(&n, 1460);

    nfs_nagle_sent(&n, 100);
    ASSERT_EQ(n.snd_nxt, 100);
    nfs_nagle_sent(&n, 200);
    ASSERT_EQ(n.snd_nxt, 300);
}

int main(void) {
    printf("Nagle & Delayed ACK Tests\n");

    test_nagle_full_mss_sends();
    test_nagle_blocks_small_when_unacked();
    test_nagle_allows_no_unacked();
    test_nagle_disabled_always_sends();
    test_nagle_toggle();
    test_nagle_ack_unblocks();
    test_nagle_mss_boundary();
    test_delayed_ack_waits();
    test_delayed_ack_second_segment();
    test_delayed_ack_sent_resets();
    test_delayed_ack_check_not_pending();
    test_delayed_ack_custom_delay();
    test_delayed_ack_full_cycle();
    test_nagle_initial_state();
    test_nagle_sent_advances();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
