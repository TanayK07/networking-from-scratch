/* Unit tests for TCP CUBIC congestion controller. */

#include "../tcp_cubic.h"
#include <math.h>
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

/* ---- init ---- */

static void test_init_cwnd(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    ASSERT_EQ(nfs_cubic_cwnd(&c), 1000);
}

static void test_init_ssthresh(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    ASSERT_EQ(c.ssthresh, 65535);
}

static void test_init_constants(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    ASSERT_TRUE(fabs(c.C - 0.4) < 0.001);
    ASSERT_TRUE(fabs(c.beta - 0.7) < 0.001);
}

static void test_init_phase(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    ASSERT_TRUE(strcmp(nfs_cubic_phase(&c), "slow_start") == 0);
}

/* ---- slow start ---- */

static void test_slow_start_growth(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);

    nfs_cubic_on_ack(&c, 0.0);
    ASSERT_EQ(nfs_cubic_cwnd(&c), 2000);
    nfs_cubic_on_ack(&c, 0.05);
    ASSERT_EQ(nfs_cubic_cwnd(&c), 3000);
    nfs_cubic_on_ack(&c, 0.10);
    ASSERT_EQ(nfs_cubic_cwnd(&c), 4000);
}

static void test_slow_start_phase(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    nfs_cubic_on_ack(&c, 0.0);
    ASSERT_TRUE(strcmp(nfs_cubic_phase(&c), "slow_start") == 0);
}

static void test_slow_start_to_avoidance(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.ssthresh = 5000;

    double t = 0.0;
    while (c.cwnd < c.ssthresh) {
        nfs_cubic_on_ack(&c, t);
        t += 0.05;
    }
    ASSERT_TRUE(strcmp(nfs_cubic_phase(&c), "congestion_avoidance") == 0);
    ASSERT_TRUE(nfs_cubic_cwnd(&c) >= 5000);
}

/* ---- loss ---- */

static void test_loss_reduces_cwnd(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 10000;
    c.ssthresh = 10000;

    nfs_cubic_on_loss(&c, 1.0);
    /* cwnd = 10000 * 0.7 = 7000 */
    ASSERT_EQ(nfs_cubic_cwnd(&c), 7000);
}

static void test_loss_sets_ssthresh(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 10000;
    c.ssthresh = 10000;

    nfs_cubic_on_loss(&c, 1.0);
    ASSERT_EQ(c.ssthresh, 7000);
}

static void test_loss_records_wmax(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 10000;
    c.ssthresh = 10000;

    nfs_cubic_on_loss(&c, 1.0);
    ASSERT_TRUE(fabs(c.wmax - 10000.0) < 1.0);
}

static void test_loss_computes_K(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 10000;
    c.ssthresh = 10000;

    nfs_cubic_on_loss(&c, 1.0);
    /* K = cbrt(Wmax * (1 - beta) / C) = cbrt(10000 * 0.3 / 0.4) = cbrt(7500) */
    double expected_K = cbrt(10000.0 * 0.3 / 0.4);
    ASSERT_TRUE(fabs(c.K - expected_K) < 0.01);
}

static void test_loss_ssthresh_floor(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 2000;
    c.ssthresh = 2000;

    nfs_cubic_on_loss(&c, 1.0);
    /* 2000*0.7 = 1400, but floor is 2*mss = 2000 */
    ASSERT_EQ(c.ssthresh, 2000);
}

/* ---- CUBIC growth curve ---- */

static void test_cubic_grows_after_loss(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 20000;
    c.ssthresh = 20000;

    nfs_cubic_on_loss(&c, 0.0);
    uint32_t post_loss = nfs_cubic_cwnd(&c); /* 14000 */
    ASSERT_EQ(post_loss, 14000);

    /* Send ACKs over time — cwnd should grow. */
    double t = 0.05;
    for (int i = 0; i < 100; i++) {
        nfs_cubic_on_ack(&c, t);
        t += 0.05;
    }
    ASSERT_TRUE(nfs_cubic_cwnd(&c) > post_loss);
}

static void test_cubic_approaches_wmax(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 50000;
    c.ssthresh = 50000;

    nfs_cubic_on_loss(&c, 0.0);
    /* Wmax = 50000, cwnd = 35000, K = cbrt(50000*0.3/0.4) ≈ 33.5s */

    /* Send enough ACKs to approach Wmax. */
    double t = 0.05;
    for (int i = 0; i < 800; i++) {
        nfs_cubic_on_ack(&c, t);
        t += 0.05;
    }
    /* After 40 seconds of ACKs, should be near or past Wmax. */
    ASSERT_TRUE(nfs_cubic_cwnd(&c) > 40000);
}

static void test_cubic_exceeds_wmax(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 20000;
    c.ssthresh = 20000;

    nfs_cubic_on_loss(&c, 0.0);
    /* Wmax = 20000, K ≈ cbrt(20000*0.3/0.4) ≈ 24.66 */

    /* Send ACKs well past K to reach convex region. */
    double t = 0.05;
    for (int i = 0; i < 1000; i++) {
        nfs_cubic_on_ack(&c, t);
        t += 0.05;
    }
    /* After 50 seconds, CUBIC should be in convex region past Wmax. */
    ASSERT_TRUE(nfs_cubic_cwnd(&c) > 20000);
}

static void test_cubic_concave_region(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 20000;
    c.ssthresh = 20000;

    nfs_cubic_on_loss(&c, 0.0); /* Wmax=20000, cwnd=14000 */

    /* Short time after loss: should be in concave region (below Wmax). */
    double t = 0.05;
    for (int i = 0; i < 10; i++) {
        nfs_cubic_on_ack(&c, t);
        t += 0.05;
    }
    /* Still below Wmax (concave phase). */
    ASSERT_TRUE(nfs_cubic_cwnd(&c) < 20000);
    ASSERT_TRUE(nfs_cubic_cwnd(&c) > 14000);
}

/* ---- multiple losses ---- */

static void test_multiple_losses(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 20000;
    c.ssthresh = 20000;

    nfs_cubic_on_loss(&c, 0.0);
    uint32_t after_first = nfs_cubic_cwnd(&c);

    /* Grow a bit */
    double t = 0.05;
    for (int i = 0; i < 10; i++) {
        nfs_cubic_on_ack(&c, t);
        t += 0.05;
    }

    nfs_cubic_on_loss(&c, t);
    uint32_t after_second = nfs_cubic_cwnd(&c);

    /* Second loss should produce a smaller cwnd (since cwnd was
     * smaller when the second loss happened). */
    ASSERT_TRUE(after_second < after_first);
}

static void test_multiple_losses_wmax_updates(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 30000;
    c.ssthresh = 30000;

    nfs_cubic_on_loss(&c, 0.0);
    double wmax1 = c.wmax;
    ASSERT_TRUE(fabs(wmax1 - 30000.0) < 1.0);

    /* Grow a bit */
    double t = 0.05;
    for (int i = 0; i < 10; i++) {
        nfs_cubic_on_ack(&c, t);
        t += 0.05;
    }

    nfs_cubic_on_loss(&c, t);
    /* New Wmax should be the cwnd at time of second loss. */
    ASSERT_TRUE(c.wmax < wmax1);
    ASSERT_TRUE(c.wmax > 0);
}

/* ---- phase transitions ---- */

static void test_phase_after_loss(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 10000;
    c.ssthresh = 10000;

    nfs_cubic_on_loss(&c, 1.0);
    /* cwnd = ssthresh = 7000, so cwnd >= ssthresh */
    ASSERT_TRUE(strcmp(nfs_cubic_phase(&c), "congestion_avoidance") == 0);
}

static void test_phase_slow_start_string(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    ASSERT_TRUE(strcmp(nfs_cubic_phase(&c), "slow_start") == 0);
}

static void test_phase_avoidance_string(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 70000;
    c.ssthresh = 65535;
    ASSERT_TRUE(strcmp(nfs_cubic_phase(&c), "congestion_avoidance") == 0);
}

/* ---- edge cases ---- */

static void test_init_different_mss(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 536);
    ASSERT_EQ(nfs_cubic_cwnd(&c), 536);
    ASSERT_EQ(c.mss, 536);
}

static void test_cwnd_getter(void) {
    struct nfs_cubic c;
    nfs_cubic_init(&c, 1000);
    c.cwnd = 42000;
    ASSERT_EQ(nfs_cubic_cwnd(&c), 42000);
}

int main(void) {
    int prev_run = 0, prev_pass = 0;

    printf("TCP CUBIC tests:\n");

    RUN(test_init_cwnd);
    RUN(test_init_ssthresh);
    RUN(test_init_constants);
    RUN(test_init_phase);

    RUN(test_slow_start_growth);
    RUN(test_slow_start_phase);
    RUN(test_slow_start_to_avoidance);

    RUN(test_loss_reduces_cwnd);
    RUN(test_loss_sets_ssthresh);
    RUN(test_loss_records_wmax);
    RUN(test_loss_computes_K);
    RUN(test_loss_ssthresh_floor);

    RUN(test_cubic_grows_after_loss);
    RUN(test_cubic_approaches_wmax);
    RUN(test_cubic_exceeds_wmax);
    RUN(test_cubic_concave_region);

    RUN(test_multiple_losses);
    RUN(test_multiple_losses_wmax_updates);

    RUN(test_phase_after_loss);
    RUN(test_phase_slow_start_string);
    RUN(test_phase_avoidance_string);

    RUN(test_init_different_mss);
    RUN(test_cwnd_getter);

    printf("\n  %d / %d passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
