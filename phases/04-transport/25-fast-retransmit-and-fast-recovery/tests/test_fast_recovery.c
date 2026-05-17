/* Unit tests for TCP fast retransmit and fast recovery (Reno). */

#include "../fast_recovery.h"
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

/* ---- initial state ---- */

static void test_initial_state(void) {
    struct nfs_fr_state s;
    nfs_fr_init(&s, 1460, 14600);

    ASSERT_EQ(nfs_fr_cwnd(&s), 14600);
    ASSERT_EQ(s.mss, 1460);
    ASSERT_EQ(s.dup_ack_count, 0);
    ASSERT_EQ(nfs_fr_in_recovery(&s), 0);
    ASSERT_EQ(s.last_ack, 0);
}

/* ---- new ACK is normal ---- */

static void test_new_ack_normal(void) {
    struct nfs_fr_state s;
    nfs_fr_init(&s, 1460, 14600);

    int r = nfs_fr_on_ack(&s, 1000);
    ASSERT_EQ(r, NFS_FR_NORMAL);
    ASSERT_EQ(s.last_ack, 1000);
    ASSERT_EQ(s.dup_ack_count, 0);
    ASSERT_EQ(nfs_fr_in_recovery(&s), 0);
}

/* ---- 1-2 dup ACKs: no action ---- */

static void test_1_2_dup_acks_no_action(void) {
    struct nfs_fr_state s;
    nfs_fr_init(&s, 1460, 14600);

    nfs_fr_on_ack(&s, 5000);

    int r = nfs_fr_on_ack(&s, 5000); /* dup 1 */
    ASSERT_EQ(r, NFS_FR_NORMAL);
    ASSERT_EQ(s.dup_ack_count, 1);
    ASSERT_EQ(nfs_fr_in_recovery(&s), 0);

    r = nfs_fr_on_ack(&s, 5000); /* dup 2 */
    ASSERT_EQ(r, NFS_FR_NORMAL);
    ASSERT_EQ(s.dup_ack_count, 2);
    ASSERT_EQ(nfs_fr_in_recovery(&s), 0);

    /* cwnd unchanged */
    ASSERT_EQ(nfs_fr_cwnd(&s), 14600);
}

/* ---- 3rd dup ACK triggers fast retransmit ---- */

static void test_3rd_dup_ack_triggers(void) {
    struct nfs_fr_state s;
    nfs_fr_init(&s, 1460, 14600);
    uint32_t original_cwnd = nfs_fr_cwnd(&s);

    nfs_fr_on_ack(&s, 5000);

    nfs_fr_on_ack(&s, 5000);         /* dup 1 */
    nfs_fr_on_ack(&s, 5000);         /* dup 2 */
    int r = nfs_fr_on_ack(&s, 5000); /* dup 3 */

    ASSERT_EQ(r, NFS_FR_FAST_RETRANSMIT);
    ASSERT_EQ(nfs_fr_in_recovery(&s), 1);
    ASSERT_EQ(s.dup_ack_count, 3);

    /* ssthresh = cwnd / 2 */
    ASSERT_EQ(s.ssthresh, original_cwnd / 2);

    /* cwnd = ssthresh + 3*MSS */
    ASSERT_EQ(nfs_fr_cwnd(&s), s.ssthresh + 3 * 1460);
}

/* ---- ssthresh is halved ---- */

static void test_ssthresh_halved(void) {
    struct nfs_fr_state s;
    nfs_fr_init(&s, 1000, 20000);

    nfs_fr_on_ack(&s, 1000);
    nfs_fr_on_ack(&s, 1000);
    nfs_fr_on_ack(&s, 1000);
    nfs_fr_on_ack(&s, 1000);

    ASSERT_EQ(s.ssthresh, 10000); /* 20000 / 2 */
}

/* ---- ssthresh minimum is 2*MSS ---- */

static void test_ssthresh_minimum(void) {
    struct nfs_fr_state s;
    nfs_fr_init(&s, 1460, 2920); /* cwnd = 2*MSS */

    nfs_fr_on_ack(&s, 100);
    nfs_fr_on_ack(&s, 100);
    nfs_fr_on_ack(&s, 100);
    nfs_fr_on_ack(&s, 100);

    /* cwnd/2 = 1460, but minimum is 2*MSS = 2920 */
    ASSERT_EQ(s.ssthresh, 2920);
}

/* ---- cwnd inflated on additional dup ACKs ---- */

static void test_cwnd_inflated(void) {
    struct nfs_fr_state s;
    nfs_fr_init(&s, 1460, 14600);

    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000); /* dup 1 */
    nfs_fr_on_ack(&s, 5000); /* dup 2 */
    nfs_fr_on_ack(&s, 5000); /* dup 3 -> fast retransmit */

    uint32_t cwnd_after_fr = nfs_fr_cwnd(&s);

    /* 4th dup ACK during recovery */
    int r = nfs_fr_on_ack(&s, 5000);
    ASSERT_EQ(r, NFS_FR_NORMAL);
    ASSERT_EQ(nfs_fr_cwnd(&s), cwnd_after_fr + 1460);

    /* 5th dup ACK */
    r = nfs_fr_on_ack(&s, 5000);
    ASSERT_EQ(r, NFS_FR_NORMAL);
    ASSERT_EQ(nfs_fr_cwnd(&s), cwnd_after_fr + 2 * 1460);
}

/* ---- new ACK covering recovery_point exits recovery ---- */

static void test_recovery_exit(void) {
    struct nfs_fr_state s;
    nfs_fr_init(&s, 1460, 14600);

    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000); /* fast retransmit */

    /* Set recovery_point as caller would */
    s.recovery_point = 10000;
    uint32_t expected_ssthresh = s.ssthresh;

    /* New ACK covering recovery_point */
    int r = nfs_fr_on_ack(&s, 10000);
    ASSERT_EQ(r, NFS_FR_RECOVERY_EXIT);
    ASSERT_EQ(nfs_fr_in_recovery(&s), 0);
    ASSERT_EQ(nfs_fr_cwnd(&s), expected_ssthresh);
    ASSERT_EQ(s.dup_ack_count, 0);
}

/* ---- partial ACK during recovery ---- */

static void test_partial_ack_during_recovery(void) {
    struct nfs_fr_state s;
    nfs_fr_init(&s, 1460, 14600);

    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000); /* enter recovery */

    s.recovery_point = 10000;

    /* Partial ACK: advances but doesn't reach recovery_point */
    int r = nfs_fr_on_ack(&s, 7000);
    ASSERT_EQ(r, NFS_FR_NORMAL);
    ASSERT_EQ(nfs_fr_in_recovery(&s), 1); /* still in recovery */
    ASSERT_EQ(s.last_ack, 7000);
}

/* ---- new ACK resets dup_ack_count ---- */

static void test_new_ack_resets_dup_count(void) {
    struct nfs_fr_state s;
    nfs_fr_init(&s, 1460, 14600);

    nfs_fr_on_ack(&s, 1000);
    nfs_fr_on_ack(&s, 1000); /* dup 1 */
    nfs_fr_on_ack(&s, 1000); /* dup 2 */
    ASSERT_EQ(s.dup_ack_count, 2);

    /* New ACK resets count */
    nfs_fr_on_ack(&s, 2000);
    ASSERT_EQ(s.dup_ack_count, 0);
}

/* ---- full Reno cycle ---- */

static void test_full_reno_cycle(void) {
    struct nfs_fr_state s;
    const uint32_t mss = 1460;
    nfs_fr_init(&s, mss, 10 * mss);

    /* Normal ACK */
    nfs_fr_on_ack(&s, 5000);
    ASSERT_EQ(nfs_fr_in_recovery(&s), 0);

    /* 3 dup ACKs -> fast retransmit */
    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000);
    int r = nfs_fr_on_ack(&s, 5000);
    ASSERT_EQ(r, NFS_FR_FAST_RETRANSMIT);
    ASSERT_EQ(nfs_fr_in_recovery(&s), 1);
    s.recovery_point = 15000;

    /* 2 more dup ACKs inflate cwnd */
    uint32_t cwnd_in_recovery = nfs_fr_cwnd(&s);
    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000);
    ASSERT_EQ(nfs_fr_cwnd(&s), cwnd_in_recovery + 2 * mss);

    /* Full ACK exits recovery */
    r = nfs_fr_on_ack(&s, 15000);
    ASSERT_EQ(r, NFS_FR_RECOVERY_EXIT);
    ASSERT_EQ(nfs_fr_in_recovery(&s), 0);

    /* cwnd deflated to ssthresh */
    ASSERT_EQ(nfs_fr_cwnd(&s), s.ssthresh);

    /* Subsequent dup ACKs can trigger another cycle */
    nfs_fr_on_ack(&s, 15000);
    nfs_fr_on_ack(&s, 15000);
    r = nfs_fr_on_ack(&s, 15000);
    ASSERT_EQ(r, NFS_FR_FAST_RETRANSMIT);
}

/* ---- dup ACK for old value after new ACK ---- */

static void test_dup_ack_different_value(void) {
    struct nfs_fr_state s;
    nfs_fr_init(&s, 1460, 14600);

    nfs_fr_on_ack(&s, 1000);
    nfs_fr_on_ack(&s, 2000);

    /* Dup of 2000, not 1000 */
    nfs_fr_on_ack(&s, 2000);
    ASSERT_EQ(s.dup_ack_count, 1);
    ASSERT_EQ(s.last_ack, 2000);
}

/* ---- recovery_point exact boundary ---- */

static void test_recovery_point_exact(void) {
    struct nfs_fr_state s;
    nfs_fr_init(&s, 1460, 14600);

    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000);

    s.recovery_point = 8000;

    /* ACK exactly at recovery_point */
    int r = nfs_fr_on_ack(&s, 8000);
    ASSERT_EQ(r, NFS_FR_RECOVERY_EXIT);
    ASSERT_EQ(nfs_fr_in_recovery(&s), 0);
}

/* ---- recovery_point above boundary ---- */

static void test_recovery_point_above(void) {
    struct nfs_fr_state s;
    nfs_fr_init(&s, 1460, 14600);

    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000);
    nfs_fr_on_ack(&s, 5000);

    s.recovery_point = 8000;

    /* ACK above recovery_point */
    int r = nfs_fr_on_ack(&s, 9000);
    ASSERT_EQ(r, NFS_FR_RECOVERY_EXIT);
}

int main(void) {
    printf("Fast Retransmit & Fast Recovery Tests\n");

    test_initial_state();
    test_new_ack_normal();
    test_1_2_dup_acks_no_action();
    test_3rd_dup_ack_triggers();
    test_ssthresh_halved();
    test_ssthresh_minimum();
    test_cwnd_inflated();
    test_recovery_exit();
    test_partial_ack_during_recovery();
    test_new_ack_resets_dup_count();
    test_full_reno_cycle();
    test_dup_ack_different_value();
    test_recovery_point_exact();
    test_recovery_point_above();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
