/* Unit tests for TCP BBR congestion control. */

#include "../bbr.h"
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

static void test_init_state(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    ASSERT_EQ(bbr.state, NFS_BBR_STARTUP);
    ASSERT_EQ(bbr.mss, 1460);
    ASSERT_EQ(bbr.cwnd, NFS_BBR_MIN_CWND * 1460u);
    ASSERT_EQ(bbr.btl_bw, 0u);
    ASSERT_TRUE(bbr.rt_prop == UINT64_MAX);
    ASSERT_EQ(bbr.delivered, 0u);
}

static void test_init_default_mss(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 0);
    ASSERT_EQ(bbr.mss, 1460);
}

static void test_init_null(void) {
    /* Should not crash */
    nfs_bbr_init(NULL, 1460);
    ASSERT_TRUE(1);
}

/* ---- Pacing gain tests ---- */

static void test_pacing_gain_startup(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    ASSERT_EQ(nfs_bbr_pacing_gain(&bbr), NFS_BBR_GAIN_STARTUP);
}

static void test_pacing_gain_probe_bw_phases(void) {
    ASSERT_EQ(nfs_bbr_probe_bw_gain(0), 1250u);
    ASSERT_EQ(nfs_bbr_probe_bw_gain(1), 750u);
    ASSERT_EQ(nfs_bbr_probe_bw_gain(2), 1000u);
    ASSERT_EQ(nfs_bbr_probe_bw_gain(7), 1000u);
    ASSERT_EQ(nfs_bbr_probe_bw_gain(8), 1000u); /* out of range */
}

static void test_cwnd_gain_startup(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    ASSERT_EQ(nfs_bbr_cwnd_gain(&bbr), 2885u);
}

/* ---- BDP tests ---- */

static void test_bdp_zero_when_no_data(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    ASSERT_EQ(nfs_bbr_bdp(&bbr), 0u);
}

static void test_bdp_calculation(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    bbr.btl_bw = 1000000; /* 1 MB/s */
    bbr.rt_prop = 20000;  /* 20ms = 20000 us */
    /* BDP = 1000000 * 20000 / 1000000 = 20000 bytes */
    ASSERT_EQ(nfs_bbr_bdp(&bbr), 20000u);
}

static void test_bdp_high_bw(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    bbr.btl_bw = 125000000; /* 1 Gbit/s in bytes */
    bbr.rt_prop = 10000;    /* 10ms */
    /* BDP = 125000000 * 10000 / 1000000 = 1250000 bytes */
    ASSERT_EQ(nfs_bbr_bdp(&bbr), 1250000u);
}

/* ---- Pacing rate tests ---- */

static void test_pacing_rate_zero(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    ASSERT_EQ(nfs_bbr_compute_pacing_rate(&bbr), 0u);
}

static void test_pacing_rate_startup(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    bbr.btl_bw = 1000000;
    /* In STARTUP: pacing_rate = 1000000 * 2885 / 1000 = 2885000 */
    ASSERT_EQ(nfs_bbr_compute_pacing_rate(&bbr), 2885000u);
}

/* ---- cwnd computation tests ---- */

static void test_cwnd_minimum(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    /* No BW data, should get minimum cwnd */
    uint32_t cwnd = nfs_bbr_compute_cwnd(&bbr);
    ASSERT_EQ(cwnd, NFS_BBR_MIN_CWND * 1460u);
}

static void test_cwnd_with_bdp(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    bbr.btl_bw = 1000000;
    bbr.rt_prop = 100000; /* 100ms */
    /* BDP = 100000 bytes, STARTUP cwnd_gain = 2885/1000
     * cwnd = 100000 * 2885 / 1000 = 288500 */
    ASSERT_EQ(nfs_bbr_compute_cwnd(&bbr), 288500u);
}

/* ---- ACK processing tests ---- */

static void test_ack_updates_rtt(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    nfs_bbr_on_ack(&bbr, 1460, 25000, 1000000, 500000);
    ASSERT_EQ(bbr.rt_prop, 25000u);
}

static void test_ack_updates_bandwidth(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    nfs_bbr_on_ack(&bbr, 1460, 20000, 1000000, 500000);
    ASSERT_EQ(bbr.btl_bw, 500000u);
    /* Higher delivery rate should update */
    nfs_bbr_on_ack(&bbr, 1460, 20000, 1050000, 800000);
    ASSERT_EQ(bbr.btl_bw, 800000u);
}

static void test_ack_min_rtt(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    nfs_bbr_on_ack(&bbr, 1460, 25000, 1000000, 500000);
    nfs_bbr_on_ack(&bbr, 1460, 20000, 1050000, 500000);
    nfs_bbr_on_ack(&bbr, 1460, 30000, 1100000, 500000);
    /* Min RTT should be 20000 */
    ASSERT_EQ(bbr.rt_prop, 20000u);
}

static void test_ack_delivered_tracking(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);
    nfs_bbr_on_ack(&bbr, 1460, 20000, 1000000, 100000);
    nfs_bbr_on_ack(&bbr, 2920, 20000, 1050000, 100000);
    ASSERT_EQ(bbr.delivered, 4380u);
}

/* ---- State transition tests ---- */

static void test_startup_to_drain(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);

    /* Simulate plateau in bandwidth for 3+ rounds */
    uint64_t now = 0;
    uint64_t rate = 1000000;

    /* First send some growing rates */
    for (int i = 0; i < 5; i++) {
        now += 50000;
        rate *= 2;
        nfs_bbr_on_ack(&bbr, 1460, 20000, now, rate);
    }

    /* Now plateau the rate -- same value for many rounds */
    for (int i = 0; i < 10; i++) {
        now += 50000;
        nfs_bbr_on_ack(&bbr, 1460, 20000, now, rate);
    }

    /* Should have transitioned out of STARTUP */
    ASSERT_TRUE(bbr.state != NFS_BBR_STARTUP);
    ASSERT_TRUE(bbr.filled_pipe);
}

static void test_probe_rtt_entry(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);

    /* Quickly get to PROBE_BW */
    bbr.state = NFS_BBR_PROBE_BW;
    bbr.btl_bw = 1000000;
    bbr.rt_prop = 20000;
    bbr.rt_prop_stamp = 0;

    /* Send ACK way past the RTprop window.
     * Use a higher RTT so rt_prop doesn't get refreshed (which would
     * reset rt_prop_stamp and prevent PROBE_RTT entry). */
    nfs_bbr_on_ack(&bbr, 1460, 30000, NFS_BBR_RTPROP_WINDOW_US + 1, 1000000);
    ASSERT_EQ(bbr.state, NFS_BBR_PROBE_RTT);
}

static void test_probe_rtt_exit(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);

    bbr.state = NFS_BBR_PROBE_RTT;
    bbr.btl_bw = 1000000;
    bbr.rt_prop = 20000;
    bbr.rt_prop_stamp = 100000000; /* recent */
    bbr.probe_rtt_done_stamp = 100000000;

    /* ACK after done stamp */
    nfs_bbr_on_ack(&bbr, 1460, 20000, 100200001, 1000000);
    ASSERT_EQ(bbr.state, NFS_BBR_PROBE_BW);
}

/* ---- State name tests ---- */

static void test_state_names(void) {
    ASSERT_TRUE(strcmp(nfs_bbr_state_name(NFS_BBR_STARTUP), "STARTUP") == 0);
    ASSERT_TRUE(strcmp(nfs_bbr_state_name(NFS_BBR_DRAIN), "DRAIN") == 0);
    ASSERT_TRUE(strcmp(nfs_bbr_state_name(NFS_BBR_PROBE_BW), "PROBE_BW") == 0);
    ASSERT_TRUE(strcmp(nfs_bbr_state_name(NFS_BBR_PROBE_RTT), "PROBE_RTT") == 0);
}

/* ---- Null safety tests ---- */

static void test_on_ack_null(void) {
    nfs_bbr_on_ack(NULL, 1460, 20000, 1000000, 500000);
    ASSERT_TRUE(1); /* no crash */
}

static void test_pacing_gain_null(void) {
    ASSERT_EQ(nfs_bbr_pacing_gain(NULL), 1000u);
}

static void test_bdp_null(void) {
    ASSERT_EQ(nfs_bbr_bdp(NULL), 0u);
}

int main(void) {
    printf("TCP BBR Tests\n");

    test_init_state();
    test_init_default_mss();
    test_init_null();
    test_pacing_gain_startup();
    test_pacing_gain_probe_bw_phases();
    test_cwnd_gain_startup();
    test_bdp_zero_when_no_data();
    test_bdp_calculation();
    test_bdp_high_bw();
    test_pacing_rate_zero();
    test_pacing_rate_startup();
    test_cwnd_minimum();
    test_cwnd_with_bdp();
    test_ack_updates_rtt();
    test_ack_updates_bandwidth();
    test_ack_min_rtt();
    test_ack_delivered_tracking();
    test_startup_to_drain();
    test_probe_rtt_entry();
    test_probe_rtt_exit();
    test_state_names();
    test_on_ack_null();
    test_pacing_gain_null();
    test_bdp_null();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
