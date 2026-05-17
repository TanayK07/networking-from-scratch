/* Unit tests for nfs_cong — TCP Congestion Control (Slow Start +
 * Congestion Avoidance, Reno-style). */

#include "../congestion.h"
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

/* ---- init tests ---- */

static void test_init_cwnd(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    ASSERT_EQ(c.cwnd, 1000);
    ASSERT_EQ(c.mss, 1000);
}

static void test_init_ssthresh(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    ASSERT_EQ(c.ssthresh, 65535);
}

static void test_init_phase(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1460);
    ASSERT_EQ(c.phase, NFS_CONG_SLOW_START);
}

static void test_init_window(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 536);
    ASSERT_EQ(nfs_cong_window(&c), 536);
}

/* ---- slow start tests ---- */

static void test_slow_start_one_ack(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    /* cwnd starts at 1000, one ACK: cwnd = 2000 */
    nfs_cong_on_ack(&c);
    ASSERT_EQ(c.cwnd, 2000);
    ASSERT_EQ(c.phase, NFS_CONG_SLOW_START);
}

static void test_slow_start_doubles_per_rtt(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    /* RTT 1: 1 segment in flight, 1 ACK => cwnd = 2000 */
    nfs_cong_on_ack(&c);
    ASSERT_EQ(c.cwnd, 2000);
    /* RTT 2: 2 segments, 2 ACKs => cwnd = 4000 */
    nfs_cong_on_ack(&c);
    nfs_cong_on_ack(&c);
    ASSERT_EQ(c.cwnd, 4000);
    /* RTT 3: 4 segments, 4 ACKs => cwnd = 8000 */
    for (int i = 0; i < 4; i++)
        nfs_cong_on_ack(&c);
    ASSERT_EQ(c.cwnd, 8000);
}

static void test_slow_start_transitions_at_ssthresh(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    c.ssthresh = 4000;
    /* cwnd=1000 -> 2000 -> 3000 -> 4000 (transition) */
    nfs_cong_on_ack(&c); /* 2000 */
    ASSERT_EQ(c.phase, NFS_CONG_SLOW_START);
    nfs_cong_on_ack(&c); /* 3000 */
    ASSERT_EQ(c.phase, NFS_CONG_SLOW_START);
    nfs_cong_on_ack(&c); /* 4000 >= ssthresh */
    ASSERT_EQ(c.phase, NFS_CONG_AVOIDANCE);
    ASSERT_EQ(c.cwnd, 4000);
}

/* ---- congestion avoidance tests ---- */

static void test_avoidance_linear_increase(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    c.cwnd = 10000;
    c.ssthresh = 10000;
    c.phase = NFS_CONG_AVOIDANCE;

    /* In avoidance: cwnd += mss*mss/cwnd = 1000000/10000 = 100 per ACK.
     * With 10 segments per RTT, cwnd increases by ~1000 per RTT. */
    uint32_t before = c.cwnd;
    uint32_t segs = c.cwnd / c.mss;
    for (uint32_t i = 0; i < segs; i++)
        nfs_cong_on_ack(&c);
    uint32_t increase = c.cwnd - before;
    /* Should increase by approximately MSS (1000) per RTT */
    ASSERT_TRUE(increase >= 900 && increase <= 1100);
}

static void test_avoidance_minimum_increment(void) {
    /* When cwnd is large, mss*mss/cwnd could be 0, so enforce minimum 1 */
    struct nfs_cong_state c;
    nfs_cong_init(&c, 100);
    c.cwnd = 100000;
    c.ssthresh = 50000;
    c.phase = NFS_CONG_AVOIDANCE;
    /* mss*mss/cwnd = 10000/100000 = 0, but should increment by at least 1 */
    uint32_t before = c.cwnd;
    nfs_cong_on_ack(&c);
    ASSERT_TRUE(c.cwnd > before);
}

static void test_avoidance_stays_in_phase(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    c.cwnd = 20000;
    c.ssthresh = 15000;
    c.phase = NFS_CONG_AVOIDANCE;
    for (int i = 0; i < 100; i++)
        nfs_cong_on_ack(&c);
    ASSERT_EQ(c.phase, NFS_CONG_AVOIDANCE);
}

/* ---- loss tests ---- */

static void test_loss_halves_ssthresh(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    c.cwnd = 20000;
    c.phase = NFS_CONG_AVOIDANCE;
    nfs_cong_on_loss(&c);
    ASSERT_EQ(c.ssthresh, 10000); /* 20000 / 2 */
}

static void test_loss_resets_cwnd(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    c.cwnd = 20000;
    nfs_cong_on_loss(&c);
    ASSERT_EQ(c.cwnd, 1000); /* back to 1 MSS */
}

static void test_loss_returns_to_slow_start(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    c.cwnd = 20000;
    c.phase = NFS_CONG_AVOIDANCE;
    nfs_cong_on_loss(&c);
    ASSERT_EQ(c.phase, NFS_CONG_SLOW_START);
}

static void test_loss_ssthresh_minimum(void) {
    /* ssthresh = max(cwnd/2, 2*mss) */
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    c.cwnd = 1000; /* cwnd/2 = 500, 2*mss = 2000 */
    nfs_cong_on_loss(&c);
    ASSERT_EQ(c.ssthresh, 2000); /* clamped to 2*MSS */
}

/* ---- triple duplicate ACK tests ---- */

static void test_triple_dup_ack_ssthresh(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    c.cwnd = 20000;
    nfs_cong_on_triple_dup_ack(&c);
    ASSERT_EQ(c.ssthresh, 10000);
}

static void test_triple_dup_ack_cwnd(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    c.cwnd = 20000;
    nfs_cong_on_triple_dup_ack(&c);
    /* cwnd = ssthresh + 3*MSS = 10000 + 3000 = 13000 */
    ASSERT_EQ(c.cwnd, 13000);
}

static void test_triple_dup_ack_minimum_ssthresh(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    c.cwnd = 2000;
    nfs_cong_on_triple_dup_ack(&c);
    /* ssthresh = max(2000/2, 2*1000) = max(1000, 2000) = 2000 */
    ASSERT_EQ(c.ssthresh, 2000);
    /* cwnd = 2000 + 3000 = 5000 */
    ASSERT_EQ(c.cwnd, 5000);
}

/* ---- phase string tests ---- */

static void test_phase_str(void) {
    ASSERT_TRUE(strcmp(nfs_cong_phase_str(NFS_CONG_SLOW_START), "slow_start") == 0);
    ASSERT_TRUE(strcmp(nfs_cong_phase_str(NFS_CONG_AVOIDANCE), "congestion_avoidance") == 0);
    ASSERT_TRUE(strcmp(nfs_cong_phase_str(99), "unknown") == 0);
}

/* ---- window function test ---- */

static void test_window_returns_cwnd(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1460);
    ASSERT_EQ(nfs_cong_window(&c), 1460);
    nfs_cong_on_ack(&c);
    ASSERT_EQ(nfs_cong_window(&c), 2920);
}

/* ---- full scenario: slow start -> avoidance -> loss -> recovery ---- */

static void test_full_lifecycle(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1000);
    c.ssthresh = 8000;

    /* Slow start to ssthresh */
    ASSERT_EQ(c.phase, NFS_CONG_SLOW_START);
    while (c.phase == NFS_CONG_SLOW_START) {
        uint32_t segs = c.cwnd / c.mss;
        for (uint32_t i = 0; i < segs && c.phase == NFS_CONG_SLOW_START; i++)
            nfs_cong_on_ack(&c);
    }
    ASSERT_EQ(c.phase, NFS_CONG_AVOIDANCE);
    ASSERT_TRUE(c.cwnd >= 8000);

    /* A few RTTs of avoidance */
    uint32_t pre_loss = c.cwnd;
    for (int rtt = 0; rtt < 5; rtt++) {
        uint32_t segs = c.cwnd / c.mss;
        for (uint32_t i = 0; i < segs; i++)
            nfs_cong_on_ack(&c);
    }
    ASSERT_TRUE(c.cwnd > pre_loss);

    /* Loss: back to slow start */
    uint32_t before_loss_cwnd = c.cwnd;
    nfs_cong_on_loss(&c);
    ASSERT_EQ(c.phase, NFS_CONG_SLOW_START);
    ASSERT_EQ(c.cwnd, 1000);
    ASSERT_EQ(c.ssthresh, before_loss_cwnd / 2);

    /* Recover */
    nfs_cong_on_ack(&c);
    ASSERT_EQ(c.cwnd, 2000);
}

/* ---- edge: MSS = 1 ---- */

static void test_mss_one(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 1);
    ASSERT_EQ(c.cwnd, 1);
    nfs_cong_on_ack(&c);
    ASSERT_EQ(c.cwnd, 2);
}

/* ---- edge: large MSS (jumbo frames) ---- */

static void test_large_mss(void) {
    struct nfs_cong_state c;
    nfs_cong_init(&c, 9000);
    ASSERT_EQ(c.cwnd, 9000);
    nfs_cong_on_ack(&c);
    ASSERT_EQ(c.cwnd, 18000);
}

int main(void) {
    printf("Congestion Control Tests\n");

    test_init_cwnd();
    test_init_ssthresh();
    test_init_phase();
    test_init_window();
    test_slow_start_one_ack();
    test_slow_start_doubles_per_rtt();
    test_slow_start_transitions_at_ssthresh();
    test_avoidance_linear_increase();
    test_avoidance_minimum_increment();
    test_avoidance_stays_in_phase();
    test_loss_halves_ssthresh();
    test_loss_resets_cwnd();
    test_loss_returns_to_slow_start();
    test_loss_ssthresh_minimum();
    test_triple_dup_ack_ssthresh();
    test_triple_dup_ack_cwnd();
    test_triple_dup_ack_minimum_ssthresh();
    test_phase_str();
    test_window_returns_cwnd();
    test_full_lifecycle();
    test_mss_one();
    test_large_mss();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
