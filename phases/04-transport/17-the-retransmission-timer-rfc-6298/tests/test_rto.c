/* Tests for RFC 6298 RTO calculator.
 *
 * Test families:
 *   1. Initial RTO is 1.0s
 *   2. First RTT sample sets SRTT
 *   3. First sample sets RTTVAR to R/2
 *   4. First sample RTO computation
 *   5. Subsequent samples converge
 *   6. Backoff doubles RTO
 *   7. Backoff clamps at 60s
 *   8. Low RTT still gets 1s minimum RTO
 *   9. High variance increases RTO
 *  10. Multiple samples converge toward stable RTT
 *  11. Large RTT sample
 *  12. RTTVAR updated before SRTT
 */

#include "rto.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

#define ASSERT_NEAR(expr, expected, epsilon)                                                       \
    do {                                                                                           \
        tests_run++;                                                                               \
        double _got = (double)(expr);                                                              \
        double _exp = (double)(expected);                                                          \
        if ((_got - _exp) > (epsilon) || (_exp - _got) > (epsilon)) {                              \
            fprintf(stderr, "  FAIL %s:%d: %s == %f, want %f (+/-%f)\n", __FILE__, __LINE__,       \
                    #expr, _got, _exp, (double)(epsilon));                                         \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

/* ---------------------------------------------------------------
 * Test 1: Initial RTO is 1.0s
 * --------------------------------------------------------------- */

static void test_initial_rto(void) {
    printf("  initial RTO...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    ASSERT_NEAR(nfs_rto_get(&s), 1.0, 0.001);
    ASSERT_EQ(s.initialized, 0);
    ASSERT_NEAR(nfs_rto_srtt(&s), 0.0, 0.001);
    ASSERT_NEAR(nfs_rto_rttvar(&s), 0.0, 0.001);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 2: First RTT sample sets SRTT
 * --------------------------------------------------------------- */

static void test_first_sample_srtt(void) {
    printf("  first sample sets SRTT...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    nfs_rto_update(&s, 0.5);
    ASSERT_NEAR(nfs_rto_srtt(&s), 0.5, 0.001);
    ASSERT_EQ(s.initialized, 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: First sample sets RTTVAR to R/2
 * --------------------------------------------------------------- */

static void test_first_sample_rttvar(void) {
    printf("  first sample RTTVAR = R/2...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    nfs_rto_update(&s, 0.8);
    ASSERT_NEAR(nfs_rto_rttvar(&s), 0.4, 0.001);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: First sample RTO computation
 * --------------------------------------------------------------- */

static void test_first_sample_rto(void) {
    printf("  first sample RTO computation...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    /* R = 0.5s
     * SRTT = 0.5, RTTVAR = 0.25
     * RTO = 0.5 + max(0, 4*0.25) = 0.5 + 1.0 = 1.5 */
    nfs_rto_update(&s, 0.5);
    ASSERT_NEAR(nfs_rto_get(&s), 1.5, 0.001);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: Second sample uses EWMA
 * --------------------------------------------------------------- */

static void test_second_sample(void) {
    printf("  second sample EWMA...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    /* First: R=0.5 => SRTT=0.5, RTTVAR=0.25 */
    nfs_rto_update(&s, 0.5);

    /* Second: R=0.6
     * RTTVAR = 0.75*0.25 + 0.25*|0.5-0.6| = 0.1875 + 0.025 = 0.2125
     * SRTT   = 0.875*0.5 + 0.125*0.6 = 0.4375 + 0.075 = 0.5125
     * RTO    = 0.5125 + 4*0.2125 = 0.5125 + 0.85 = 1.3625 */
    nfs_rto_update(&s, 0.6);
    ASSERT_NEAR(nfs_rto_srtt(&s), 0.5125, 0.001);
    ASSERT_NEAR(nfs_rto_rttvar(&s), 0.2125, 0.001);
    ASSERT_NEAR(nfs_rto_get(&s), 1.3625, 0.001);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: Backoff doubles RTO
 * --------------------------------------------------------------- */

static void test_backoff_doubles(void) {
    printf("  backoff doubles RTO...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    nfs_rto_update(&s, 0.5);
    double rto1 = nfs_rto_get(&s);
    ASSERT_NEAR(rto1, 1.5, 0.001);

    nfs_rto_backoff(&s);
    ASSERT_NEAR(nfs_rto_get(&s), 3.0, 0.001);

    nfs_rto_backoff(&s);
    ASSERT_NEAR(nfs_rto_get(&s), 6.0, 0.001);

    nfs_rto_backoff(&s);
    ASSERT_NEAR(nfs_rto_get(&s), 12.0, 0.001);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: Backoff clamps at 60s
 * --------------------------------------------------------------- */

static void test_backoff_clamp(void) {
    printf("  backoff clamps at 60s...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    /* Start with a large RTO */
    nfs_rto_update(&s, 5.0);
    /* SRTT=5.0, RTTVAR=2.5, RTO = 5+4*2.5 = 15.0 */
    ASSERT_NEAR(nfs_rto_get(&s), 15.0, 0.001);

    /* Backoff: 15->30->60->60 */
    nfs_rto_backoff(&s);
    ASSERT_NEAR(nfs_rto_get(&s), 30.0, 0.001);

    nfs_rto_backoff(&s);
    ASSERT_NEAR(nfs_rto_get(&s), 60.0, 0.001);

    nfs_rto_backoff(&s);
    ASSERT_NEAR(nfs_rto_get(&s), 60.0, 0.001); /* clamped */

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: Low RTT still gets 1s minimum RTO
 * --------------------------------------------------------------- */

static void test_low_rtt_minimum(void) {
    printf("  low RTT minimum RTO...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    /* Very low RTT: 1ms */
    nfs_rto_update(&s, 0.001);
    /* SRTT=0.001, RTTVAR=0.0005, RTO = 0.001+4*0.0005 = 0.003
     * But clamped to 1.0 */
    ASSERT_NEAR(nfs_rto_get(&s), 1.0, 0.001);

    /* Feed more low samples — RTO should stay at 1.0 */
    for (int i = 0; i < 20; i++)
        nfs_rto_update(&s, 0.001);
    ASSERT_NEAR(nfs_rto_get(&s), 1.0, 0.001);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: High variance increases RTO
 * --------------------------------------------------------------- */

static void test_high_variance(void) {
    printf("  high variance increases RTO...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    nfs_rto_update(&s, 0.5);
    double rto_stable = nfs_rto_get(&s);

    /* Inject a wildly different sample */
    nfs_rto_update(&s, 2.0);
    double rto_spike = nfs_rto_get(&s);

    /* RTO should have increased because RTTVAR went up */
    ASSERT_TRUE(rto_spike > rto_stable);

    /* RTTVAR should have increased due to the large delta */
    ASSERT_TRUE(nfs_rto_rttvar(&s) > 0.25);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 10: Multiple samples converge toward stable RTT
 * --------------------------------------------------------------- */

static void test_convergence(void) {
    printf("  convergence toward stable RTT...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    /* Feed 50 identical samples at 100ms */
    for (int i = 0; i < 50; i++)
        nfs_rto_update(&s, 0.1);

    /* SRTT should converge very close to 0.1 */
    ASSERT_NEAR(nfs_rto_srtt(&s), 0.1, 0.001);

    /* RTTVAR should converge toward 0 (no variance) */
    ASSERT_TRUE(nfs_rto_rttvar(&s) < 0.001);

    /* RTO should be at minimum (1.0s) since 0.1 + 4*~0 < 1.0 */
    ASSERT_NEAR(nfs_rto_get(&s), 1.0, 0.001);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 11: Large RTT sample
 * --------------------------------------------------------------- */

static void test_large_rtt(void) {
    printf("  large RTT sample...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    /* 10 second RTT */
    nfs_rto_update(&s, 10.0);
    /* SRTT=10, RTTVAR=5, RTO=10+4*5=30 */
    ASSERT_NEAR(nfs_rto_srtt(&s), 10.0, 0.001);
    ASSERT_NEAR(nfs_rto_rttvar(&s), 5.0, 0.001);
    ASSERT_NEAR(nfs_rto_get(&s), 30.0, 0.001);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 12: RTO max clamp on extreme values
 * --------------------------------------------------------------- */

static void test_rto_max_clamp(void) {
    printf("  RTO max clamp...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    /* 50 second RTT => RTO = 50+4*25 = 150, clamped to 60 */
    nfs_rto_update(&s, 50.0);
    ASSERT_NEAR(nfs_rto_get(&s), 60.0, 0.001);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 13: Backoff from initial state
 * --------------------------------------------------------------- */

static void test_backoff_from_initial(void) {
    printf("  backoff from initial state...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    /* Initial RTO = 1.0 */
    ASSERT_NEAR(nfs_rto_get(&s), 1.0, 0.001);

    nfs_rto_backoff(&s);
    ASSERT_NEAR(nfs_rto_get(&s), 2.0, 0.001);

    nfs_rto_backoff(&s);
    ASSERT_NEAR(nfs_rto_get(&s), 4.0, 0.001);

    nfs_rto_backoff(&s);
    ASSERT_NEAR(nfs_rto_get(&s), 8.0, 0.001);

    nfs_rto_backoff(&s);
    ASSERT_NEAR(nfs_rto_get(&s), 16.0, 0.001);

    nfs_rto_backoff(&s);
    ASSERT_NEAR(nfs_rto_get(&s), 32.0, 0.001);

    nfs_rto_backoff(&s);
    ASSERT_NEAR(nfs_rto_get(&s), 60.0, 0.001); /* clamped */

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 14: Update after backoff resets based on measurement
 * --------------------------------------------------------------- */

static void test_update_after_backoff(void) {
    printf("  update after backoff...");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    nfs_rto_update(&s, 0.5);
    nfs_rto_backoff(&s);
    nfs_rto_backoff(&s);

    /* RTO should be 6.0 (1.5 * 2 * 2) */
    ASSERT_NEAR(nfs_rto_get(&s), 6.0, 0.001);

    /* New measurement should recalculate RTO from SRTT/RTTVAR, not from backed-off value */
    nfs_rto_update(&s, 0.5);
    /* SRTT and RTTVAR should be reasonable, not reflecting backoff */
    ASSERT_TRUE(nfs_rto_srtt(&s) < 1.0);
    ASSERT_TRUE(nfs_rto_get(&s) < 6.0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void) {
    printf("RFC 6298 RTO calculator test suite\n");

    test_initial_rto();
    test_first_sample_srtt();
    test_first_sample_rttvar();
    test_first_sample_rto();
    test_second_sample();
    test_backoff_doubles();
    test_backoff_clamp();
    test_low_rtt_minimum();
    test_high_variance();
    test_convergence();
    test_large_rtt();
    test_rto_max_clamp();
    test_backoff_from_initial();
    test_update_after_backoff();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
