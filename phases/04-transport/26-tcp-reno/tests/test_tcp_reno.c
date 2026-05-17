/* Unit tests for TCP Reno congestion controller. */

#include "../tcp_reno.h"
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

#define RUN(fn)                                                                                    \
    do {                                                                                           \
        fn();                                                                                      \
        printf("  %-50s %s\n", #fn,                                                                \
               (tests_passed == prev_pass + (tests_run - prev_run)) ? "PASS" : "FAIL");            \
        prev_run = tests_run;                                                                      \
        prev_pass = tests_passed;                                                                  \
    } while (0)

/* ---- init tests ---- */

static void test_init_cwnd(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1460);
    ASSERT_EQ(nfs_reno_cwnd(&r), 1460);
}

static void test_init_ssthresh(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1460);
    ASSERT_EQ(r.ssthresh, 65535);
}

static void test_init_mss(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 512);
    ASSERT_EQ(r.mss, 512);
    ASSERT_EQ(nfs_reno_cwnd(&r), 512);
}

static void test_init_phase(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1460);
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "slow_start") == 0);
}

static void test_init_recovery_state(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1460);
    ASSERT_EQ(r.in_recovery, 0);
    ASSERT_EQ(r.dup_ack_count, 0);
}

/* ---- slow start tests ---- */

static void test_slow_start_single_ack(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    nfs_reno_on_ack(&r, 1000);
    ASSERT_EQ(nfs_reno_cwnd(&r), 2000);
}

static void test_slow_start_exponential(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    /* Each ACK adds mss — exponential when one ACK per mss */
    nfs_reno_on_ack(&r, 1000); /* 2000 */
    ASSERT_EQ(nfs_reno_cwnd(&r), 2000);
    nfs_reno_on_ack(&r, 1000); /* 3000 */
    ASSERT_EQ(nfs_reno_cwnd(&r), 3000);
    nfs_reno_on_ack(&r, 1000); /* 4000 */
    ASSERT_EQ(nfs_reno_cwnd(&r), 4000);
    nfs_reno_on_ack(&r, 1000); /* 5000 */
    ASSERT_EQ(nfs_reno_cwnd(&r), 5000);
}

static void test_slow_start_phase_string(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    nfs_reno_on_ack(&r, 1000);
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "slow_start") == 0);
}

/* ---- transition to congestion avoidance ---- */

static void test_transition_to_avoidance(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.ssthresh = 4000;

    /* Grow through slow start */
    nfs_reno_on_ack(&r, 1000); /* 2000 */
    nfs_reno_on_ack(&r, 1000); /* 3000 */
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "slow_start") == 0);

    nfs_reno_on_ack(&r, 1000); /* 4000 = ssthresh */
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "congestion_avoidance") == 0);
}

/* ---- congestion avoidance linear growth ---- */

static void test_avoidance_linear_growth(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 10000;
    r.ssthresh = 10000;

    uint32_t before = nfs_reno_cwnd(&r);
    nfs_reno_on_ack(&r, 1000);
    uint32_t after = nfs_reno_cwnd(&r);

    /* In avoidance: increment = mss*mss/cwnd = 1000*1000/10000 = 100 */
    ASSERT_EQ(after - before, 100);
}

static void test_avoidance_sub_mss_growth(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 50000;
    r.ssthresh = 50000;

    uint32_t before = nfs_reno_cwnd(&r);
    nfs_reno_on_ack(&r, 1000);
    uint32_t after = nfs_reno_cwnd(&r);

    /* 1000*1000/50000 = 20 */
    ASSERT_EQ(after - before, 20);
}

static void test_avoidance_phase_string(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 10000;
    r.ssthresh = 10000;
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "congestion_avoidance") == 0);
}

/* ---- triple dup ACK / fast retransmit ---- */

static void test_dup_ack_no_retransmit_at_1(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 10000;
    r.ssthresh = 10000;

    ASSERT_EQ(nfs_reno_on_dup_ack(&r), 0);
    ASSERT_EQ(r.dup_ack_count, 1);
}

static void test_dup_ack_no_retransmit_at_2(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 10000;
    r.ssthresh = 10000;

    nfs_reno_on_dup_ack(&r);
    ASSERT_EQ(nfs_reno_on_dup_ack(&r), 0);
    ASSERT_EQ(r.dup_ack_count, 2);
}

static void test_dup_ack_retransmit_at_3(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 10000;
    r.ssthresh = 10000;

    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);
    int ret = nfs_reno_on_dup_ack(&r);
    ASSERT_EQ(ret, 1);
    ASSERT_EQ(r.in_recovery, 1);
}

static void test_fast_retransmit_ssthresh(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 10000;
    r.ssthresh = 10000;

    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);

    /* ssthresh = max(cwnd/2, 2*mss) = max(5000, 2000) = 5000 */
    ASSERT_EQ(r.ssthresh, 5000);
}

static void test_fast_retransmit_cwnd(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 10000;
    r.ssthresh = 10000;

    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);

    /* cwnd = ssthresh + 3*mss = 5000 + 3000 = 8000 */
    ASSERT_EQ(nfs_reno_cwnd(&r), 8000);
}

static void test_fast_retransmit_phase(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 10000;
    r.ssthresh = 10000;

    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);

    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "fast_recovery") == 0);
}

/* ---- fast recovery inflation ---- */

static void test_recovery_inflation(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 10000;
    r.ssthresh = 10000;

    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r); /* cwnd = 8000 */

    uint32_t before = nfs_reno_cwnd(&r);
    nfs_reno_on_dup_ack(&r); /* 4th dup ACK: +1000 */
    ASSERT_EQ(nfs_reno_cwnd(&r), before + 1000);

    nfs_reno_on_dup_ack(&r); /* 5th dup ACK: +1000 */
    ASSERT_EQ(nfs_reno_cwnd(&r), before + 2000);
}

static void test_recovery_still_in_recovery(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 10000;
    r.ssthresh = 10000;

    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);

    ASSERT_EQ(r.in_recovery, 1);
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "fast_recovery") == 0);
}

/* ---- exit recovery ---- */

static void test_exit_recovery_deflates(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 10000;
    r.ssthresh = 10000;

    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r); /* ssthresh = 5000, cwnd = 8000 */

    nfs_reno_on_dup_ack(&r); /* cwnd = 9000 */
    nfs_reno_on_dup_ack(&r); /* cwnd = 10000 */

    nfs_reno_exit_recovery(&r);
    /* cwnd = ssthresh = 5000 */
    ASSERT_EQ(nfs_reno_cwnd(&r), 5000);
    ASSERT_EQ(r.in_recovery, 0);
    ASSERT_EQ(r.dup_ack_count, 0);
}

static void test_exit_recovery_via_new_ack(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 10000;
    r.ssthresh = 10000;

    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r); /* ssthresh = 5000 */

    /* New ACK during recovery should exit recovery */
    nfs_reno_on_ack(&r, 1000);
    ASSERT_EQ(r.in_recovery, 0);
    ASSERT_EQ(nfs_reno_cwnd(&r), 5000);
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "congestion_avoidance") == 0);
}

/* ---- timeout ---- */

static void test_timeout_resets_cwnd(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 20000;
    r.ssthresh = 20000;

    nfs_reno_on_timeout(&r);
    ASSERT_EQ(nfs_reno_cwnd(&r), 1000);
}

static void test_timeout_sets_ssthresh(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 20000;
    r.ssthresh = 20000;

    nfs_reno_on_timeout(&r);
    /* ssthresh = max(cwnd/2, 2*mss) = max(10000, 2000) = 10000 */
    ASSERT_EQ(r.ssthresh, 10000);
}

static void test_timeout_back_to_slow_start(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 20000;
    r.ssthresh = 20000;

    nfs_reno_on_timeout(&r);
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "slow_start") == 0);
}

static void test_timeout_clears_recovery(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 10000;
    r.ssthresh = 10000;

    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);
    ASSERT_EQ(r.in_recovery, 1);

    nfs_reno_on_timeout(&r);
    ASSERT_EQ(r.in_recovery, 0);
    ASSERT_EQ(r.dup_ack_count, 0);
}

static void test_timeout_ssthresh_floor(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 1000; /* Very small cwnd */

    nfs_reno_on_timeout(&r);
    /* ssthresh = max(500, 2000) = 2000 */
    ASSERT_EQ(r.ssthresh, 2000);
}

/* ---- phase strings ---- */

static void test_phase_slow_start(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "slow_start") == 0);
}

static void test_phase_avoidance(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.cwnd = 65535;
    r.ssthresh = 65535;
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "congestion_avoidance") == 0);
}

static void test_phase_fast_recovery(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.in_recovery = 1;
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "fast_recovery") == 0);
}

/* ---- full scenario ---- */

static void test_full_scenario(void) {
    struct nfs_reno r;
    nfs_reno_init(&r, 1000);
    r.ssthresh = 5000;

    /* Slow start to 5000 */
    for (int i = 0; i < 4; i++)
        nfs_reno_on_ack(&r, 1000);
    ASSERT_EQ(nfs_reno_cwnd(&r), 5000);
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "congestion_avoidance") == 0);

    /* A few avoidance ACKs */
    nfs_reno_on_ack(&r, 1000);
    ASSERT_TRUE(nfs_reno_cwnd(&r) > 5000);
    ASSERT_TRUE(nfs_reno_cwnd(&r) < 6000);

    /* Triple dup ACK */
    nfs_reno_on_dup_ack(&r);
    nfs_reno_on_dup_ack(&r);
    int ret = nfs_reno_on_dup_ack(&r);
    ASSERT_EQ(ret, 1);
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "fast_recovery") == 0);

    /* Exit recovery */
    nfs_reno_exit_recovery(&r);
    ASSERT_TRUE(strcmp(nfs_reno_phase(&r), "congestion_avoidance") == 0);
}

int main(void) {
    int prev_run = 0, prev_pass = 0;

    printf("TCP Reno tests:\n");

    RUN(test_init_cwnd);
    RUN(test_init_ssthresh);
    RUN(test_init_mss);
    RUN(test_init_phase);
    RUN(test_init_recovery_state);

    RUN(test_slow_start_single_ack);
    RUN(test_slow_start_exponential);
    RUN(test_slow_start_phase_string);

    RUN(test_transition_to_avoidance);

    RUN(test_avoidance_linear_growth);
    RUN(test_avoidance_sub_mss_growth);
    RUN(test_avoidance_phase_string);

    RUN(test_dup_ack_no_retransmit_at_1);
    RUN(test_dup_ack_no_retransmit_at_2);
    RUN(test_dup_ack_retransmit_at_3);
    RUN(test_fast_retransmit_ssthresh);
    RUN(test_fast_retransmit_cwnd);
    RUN(test_fast_retransmit_phase);

    RUN(test_recovery_inflation);
    RUN(test_recovery_still_in_recovery);

    RUN(test_exit_recovery_deflates);
    RUN(test_exit_recovery_via_new_ack);

    RUN(test_timeout_resets_cwnd);
    RUN(test_timeout_sets_ssthresh);
    RUN(test_timeout_back_to_slow_start);
    RUN(test_timeout_clears_recovery);
    RUN(test_timeout_ssthresh_floor);

    RUN(test_phase_slow_start);
    RUN(test_phase_avoidance);
    RUN(test_phase_fast_recovery);

    RUN(test_full_scenario);

    printf("\n  %d / %d passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
