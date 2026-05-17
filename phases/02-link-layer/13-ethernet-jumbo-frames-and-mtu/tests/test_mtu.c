/* Unit tests for MTU, jumbo frames, and frame size limits. */

#include "../mtu.h"

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

#define ASSERT_NEAR(expr, expected, tol)                                                           \
    do {                                                                                           \
        tests_run++;                                                                               \
        double _got = (double)(expr);                                                              \
        double _exp = (double)(expected);                                                          \
        if ((_got - _exp) > (tol) || (_exp - _got) > (tol)) {                                      \
            fprintf(stderr, "  FAIL %s:%d: %s == %.6f, want %.6f\n", __FILE__, __LINE__, #expr,    \
                    _got, _exp);                                                                   \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

/* ------------------------------------------------------------------ */
/*  Test 1: Standard frame constants                                   */
/* ------------------------------------------------------------------ */
static void test_standard_frame_constants(void) {
    ASSERT_EQ(NFS_ETH_MIN_FRAME, 64);
    ASSERT_EQ(NFS_ETH_MAX_FRAME, 1518);
    ASSERT_EQ(NFS_ETH_MAX_PAYLOAD, 1500);
}

/* ------------------------------------------------------------------ */
/*  Test 2: Jumbo constants                                            */
/* ------------------------------------------------------------------ */
static void test_jumbo_constants(void) {
    ASSERT_EQ(NFS_JUMBO_MTU, 9000);
    ASSERT_EQ(NFS_JUMBO_MAX_FRAME, 9018);
}

/* ------------------------------------------------------------------ */
/*  Test 3: VLAN constants                                             */
/* ------------------------------------------------------------------ */
static void test_vlan_constants(void) {
    ASSERT_EQ(NFS_VLAN_TAG_LEN, 4);
    ASSERT_EQ(NFS_VLAN_MAX_FRAME, 1522);
}

/* ------------------------------------------------------------------ */
/*  Test 4: PHY overhead                                               */
/* ------------------------------------------------------------------ */
static void test_phy_overhead(void) {
    ASSERT_EQ(NFS_PREAMBLE_LEN + NFS_SFD_LEN + NFS_IFG_LEN, 20);
    ASSERT_EQ(NFS_PHY_OVERHEAD, 20);
}

/* ------------------------------------------------------------------ */
/*  Test 5: Frame valid -- min standard                                */
/* ------------------------------------------------------------------ */
static void test_frame_valid_min_standard(void) {
    ASSERT_EQ(nfs_mtu_frame_valid(64, 1500), 1);
}

/* ------------------------------------------------------------------ */
/*  Test 6: Frame valid -- max standard                                */
/* ------------------------------------------------------------------ */
static void test_frame_valid_max_standard(void) {
    ASSERT_EQ(nfs_mtu_frame_valid(1518, 1500), 1);
}

/* ------------------------------------------------------------------ */
/*  Test 7: Frame valid -- too small                                   */
/* ------------------------------------------------------------------ */
static void test_frame_valid_too_small(void) {
    ASSERT_EQ(nfs_mtu_frame_valid(63, 1500), 0);
}

/* ------------------------------------------------------------------ */
/*  Test 8: Frame valid -- too large for MTU                           */
/* ------------------------------------------------------------------ */
static void test_frame_valid_too_large(void) {
    ASSERT_EQ(nfs_mtu_frame_valid(1519, 1500), 0);
}

/* ------------------------------------------------------------------ */
/*  Test 9: Frame valid -- jumbo ok                                    */
/* ------------------------------------------------------------------ */
static void test_frame_valid_jumbo(void) {
    ASSERT_EQ(nfs_mtu_frame_valid(9018, 9000), 1);
}

/* ------------------------------------------------------------------ */
/*  Test 10: Payload fits                                              */
/* ------------------------------------------------------------------ */
static void test_payload_fits(void) {
    ASSERT_EQ(nfs_mtu_payload_fits(1500, 1500), 1);
    ASSERT_EQ(nfs_mtu_payload_fits(1501, 1500), 0);
}

/* ------------------------------------------------------------------ */
/*  Test 11: Needs fragmentation                                       */
/* ------------------------------------------------------------------ */
static void test_needs_fragmentation(void) {
    ASSERT_EQ(nfs_mtu_needs_fragmentation(1501, 1500), 1);
    ASSERT_EQ(nfs_mtu_needs_fragmentation(1500, 1500), 0);
}

/* ------------------------------------------------------------------ */
/*  Test 12: Efficiency min payload (46B)                              */
/* ------------------------------------------------------------------ */
static void test_efficiency_min_payload(void) {
    /* 46 / (20 + 14 + 46 + 4) = 46 / 84 = 0.547619... */
    ASSERT_NEAR(nfs_mtu_efficiency(46), 46.0 / 84.0, 0.0001);
}

/* ------------------------------------------------------------------ */
/*  Test 13: Efficiency max standard (1500B)                           */
/* ------------------------------------------------------------------ */
static void test_efficiency_max_standard(void) {
    /* 1500 / (20 + 14 + 1500 + 4) = 1500 / 1538 = 0.97529... */
    ASSERT_NEAR(nfs_mtu_efficiency(1500), 1500.0 / 1538.0, 0.0001);
}

/* ------------------------------------------------------------------ */
/*  Test 14: Efficiency jumbo (9000B)                                  */
/* ------------------------------------------------------------------ */
static void test_efficiency_jumbo(void) {
    /* 9000 / (20 + 14 + 9000 + 4) = 9000 / 9038 = 0.99579... */
    ASSERT_NEAR(nfs_mtu_efficiency(9000), 9000.0 / 9038.0, 0.0001);
}

/* ------------------------------------------------------------------ */
/*  Test 15: Efficiency small payload (1B)                             */
/* ------------------------------------------------------------------ */
static void test_efficiency_small_payload(void) {
    /* 1 / (20 + 14 + 46 + 4) = 1 / 84 = 0.01190... */
    ASSERT_NEAR(nfs_mtu_efficiency(1), 1.0 / 84.0, 0.0001);
}

/* ------------------------------------------------------------------ */
/*  Test 16: Max FPS 64B at 1Gbps                                     */
/* ------------------------------------------------------------------ */
static void test_max_fps_64b_1g(void) {
    /* 1e9 / ((64 + 20) * 8) = 1e9 / 672 = 1488095.238... -> 1488095 */
    size_t fps = nfs_mtu_max_fps(64, 1000);
    ASSERT_EQ(fps, 1488095);
}

/* ------------------------------------------------------------------ */
/*  Test 17: Max FPS 1518B at 1Gbps                                   */
/* ------------------------------------------------------------------ */
static void test_max_fps_1518b_1g(void) {
    /* 1e9 / ((1518 + 20) * 8) = 1e9 / 12304 = 81274.38... -> 81274 */
    size_t fps = nfs_mtu_max_fps(1518, 1000);
    ASSERT_EQ(fps, 81274);
}

/* ------------------------------------------------------------------ */
/*  Test 18: Max FPS 64B at 10Gbps                                    */
/* ------------------------------------------------------------------ */
static void test_max_fps_64b_10g(void) {
    /* 10e9 / ((64 + 20) * 8) = 10e9 / 672 = 14880952.38... -> 14880952 */
    size_t fps = nfs_mtu_max_fps(64, 10000);
    ASSERT_EQ(fps, 14880952);
}

/* ------------------------------------------------------------------ */
/*  Test 19: Goodput 1500B at 1Gbps                                   */
/* ------------------------------------------------------------------ */
static void test_goodput_1500b_1g(void) {
    /*
     * wire = 20 + 14 + 1500 + 4 = 1538 bytes
     * FPS  = 1e9 / (1538 * 8)   = 81274.38...
     * goodput = 81274.38... * 1500 * 8 = 975,292,588... bps ~ 975.3 Mbps
     */
    double gp = nfs_mtu_goodput_bps(1500, 1000);
    double expected = (1e9 / (1538.0 * 8.0)) * 1500.0 * 8.0;
    ASSERT_NEAR(gp, expected, 1.0);
}

/* ------------------------------------------------------------------ */
/*  Test 20: Path MTU                                                  */
/* ------------------------------------------------------------------ */
static void test_path_mtu(void) {
    size_t links[] = {1500, 9000, 1500, 576};
    ASSERT_EQ(nfs_path_mtu(links, 4), 576);
}

/* ------------------------------------------------------------------ */
/*  Test 21: Path MTU single link                                      */
/* ------------------------------------------------------------------ */
static void test_path_mtu_single(void) {
    size_t links[] = {1500};
    ASSERT_EQ(nfs_path_mtu(links, 1), 1500);
}

/* ------------------------------------------------------------------ */
/*  Test 22: MTU clamp -- above max                                    */
/* ------------------------------------------------------------------ */
static void test_mtu_clamp_above(void) {
    ASSERT_EQ(nfs_mtu_clamp(9000, 68, 1500), 1500);
}

/* ------------------------------------------------------------------ */
/*  Test 23: MTU clamp -- below min                                    */
/* ------------------------------------------------------------------ */
static void test_mtu_clamp_below(void) {
    ASSERT_EQ(nfs_mtu_clamp(40, 68, 1500), 68);
}

/* ------------------------------------------------------------------ */
/*  Test 24: Standard profile                                          */
/* ------------------------------------------------------------------ */
static void test_standard_profile(void) {
    struct nfs_mtu_profile p = nfs_mtu_standard();
    ASSERT_TRUE(strcmp(p.name, "standard") == 0);
    ASSERT_EQ(p.mtu, 1500);
    ASSERT_EQ(p.max_frame, 1518);
}

/* ------------------------------------------------------------------ */
/*  Test 25: Jumbo profile                                             */
/* ------------------------------------------------------------------ */
static void test_jumbo_profile(void) {
    struct nfs_mtu_profile p = nfs_mtu_jumbo();
    ASSERT_TRUE(strcmp(p.name, "jumbo") == 0);
    ASSERT_EQ(p.mtu, 9000);
    ASSERT_EQ(p.max_frame, 9018);
}

/* ------------------------------------------------------------------ */
/*  Test 26: Min frame breakdown                                       */
/* ------------------------------------------------------------------ */
static void test_min_frame_breakdown(void) {
    ASSERT_EQ(NFS_ETH_HDR_LEN + NFS_ETH_MIN_PAYLOAD + NFS_ETH_FCS_LEN, 64);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_standard_frame_constants();
    test_jumbo_constants();
    test_vlan_constants();
    test_phy_overhead();
    test_frame_valid_min_standard();
    test_frame_valid_max_standard();
    test_frame_valid_too_small();
    test_frame_valid_too_large();
    test_frame_valid_jumbo();
    test_payload_fits();
    test_needs_fragmentation();
    test_efficiency_min_payload();
    test_efficiency_max_standard();
    test_efficiency_jumbo();
    test_efficiency_small_payload();
    test_max_fps_64b_1g();
    test_max_fps_1518b_1g();
    test_max_fps_64b_10g();
    test_goodput_1500b_1g();
    test_path_mtu();
    test_path_mtu_single();
    test_mtu_clamp_above();
    test_mtu_clamp_below();
    test_standard_profile();
    test_jumbo_profile();
    test_min_frame_breakdown();

    printf("\n  %d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        fprintf(stderr, "  SOME TESTS FAILED\n");
        return 1;
    }
    printf("  All tests passed.\n");
    return 0;
}
