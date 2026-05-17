/* Unit tests for What's Actually on the Wire. */

#include "../wire.h"
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
            fprintf(stderr, "  FAIL %s:%d: %s == %.6f, want %.6f (tol %.6f)\n", __FILE__,          \
                    __LINE__, #expr, _got, _exp, (double)(tol));                                   \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

/* ---- 1. Preamble generation ---- */

static void test_preamble_generate(void) {
    uint8_t buf[8];
    int n = nfs_preamble_generate(buf, sizeof(buf));
    ASSERT_EQ(n, 8);
    for (int i = 0; i < 7; i++) {
        ASSERT_EQ(buf[i], 0xAA);
    }
    ASSERT_EQ(buf[7], 0xAB);
}

/* ---- 2. Preamble detection ---- */

static void test_preamble_detect(void) {
    uint8_t buf[8];
    nfs_preamble_generate(buf, sizeof(buf));
    int offset = nfs_preamble_detect(buf, sizeof(buf));
    /* SFD is at index 7 (the 8th byte) */
    ASSERT_EQ(offset, 7);
}

/* ---- 3. Preamble detection with noise prefix ---- */

static void test_preamble_detect_noise_prefix(void) {
    uint8_t wire[16];
    memset(wire, 0x42, sizeof(wire));

    /* Insert preamble starting at offset 3 */
    uint8_t preamble[8];
    nfs_preamble_generate(preamble, sizeof(preamble));
    memcpy(wire + 3, preamble, 8);

    int offset = nfs_preamble_detect(wire, sizeof(wire));
    /* SFD is at position 3 + 7 = 10 */
    ASSERT_EQ(offset, 10);
}

/* ---- 4. Preamble not found ---- */

static void test_preamble_not_found(void) {
    uint8_t buf[16];
    memset(buf, 0x55, sizeof(buf));
    int offset = nfs_preamble_detect(buf, sizeof(buf));
    ASSERT_EQ(offset, -1);
}

/* ---- 5. Preamble buffer too small ---- */

static void test_preamble_buffer_too_small(void) {
    uint8_t buf[4];
    int n = nfs_preamble_generate(buf, sizeof(buf));
    ASSERT_EQ(n, 0);
}

/* ---- 6. IFG bytes always 12 ---- */

static void test_ifg_bytes(void) {
    ASSERT_EQ(nfs_ifg_bytes(10), 12);
    ASSERT_EQ(nfs_ifg_bytes(100), 12);
    ASSERT_EQ(nfs_ifg_bytes(1000), 12);
    ASSERT_EQ(nfs_ifg_bytes(10000), 12);
}

/* ---- 7. IFG timing 10 Mbps ---- */

static void test_ifg_ns_10mbps(void) {
    ASSERT_EQ(nfs_ifg_ns(10), 9600);
}

/* ---- 8. IFG timing 100 Mbps ---- */

static void test_ifg_ns_100mbps(void) {
    ASSERT_EQ(nfs_ifg_ns(100), 960);
}

/* ---- 9. IFG timing 1 Gbps ---- */

static void test_ifg_ns_1gbps(void) {
    ASSERT_EQ(nfs_ifg_ns(1000), 96);
}

/* ---- 10. Bit time 10 Mbps ---- */

static void test_bit_time_10mbps(void) {
    ASSERT_EQ(nfs_bit_time_ns(10), 100);
}

/* ---- 11. Bit time 100 Mbps ---- */

static void test_bit_time_100mbps(void) {
    ASSERT_EQ(nfs_bit_time_ns(100), 10);
}

/* ---- 12. Bit time 1 Gbps ---- */

static void test_bit_time_1gbps(void) {
    ASSERT_EQ(nfs_bit_time_ns(1000), 1);
}

/* ---- 13. RD update: balanced symbol (5 ones) ---- */

static void test_rd_balanced(void) {
    /* 0x1F = 0b0000011111, lower 10 bits: 00 0001 1111 = 5 ones */
    int rd = -1;
    rd = nfs_rd_update(rd, 0x01F);
    ASSERT_EQ(rd, -1); /* unchanged */

    rd = 1;
    rd = nfs_rd_update(rd, 0x01F);
    ASSERT_EQ(rd, 1); /* unchanged */
}

/* ---- 14. RD update: more ones → RD becomes +1 ---- */

static void test_rd_more_ones(void) {
    /* 0x3FF = 0b1111111111 = 10 ones */
    int rd = -1;
    rd = nfs_rd_update(rd, 0x3FF);
    ASSERT_EQ(rd, 1);
}

/* ---- 15. RD update: fewer ones → RD becomes -1 ---- */

static void test_rd_fewer_ones(void) {
    /* 0x000 = 0 ones */
    int rd = 1;
    rd = nfs_rd_update(rd, 0x000);
    ASSERT_EQ(rd, -1);
}

/* ---- 16. DC balance check: 5 ones ---- */

static void test_dc_balanced(void) {
    /* 0x01F = 00 0001 1111 = 5 ones → balanced */
    ASSERT_EQ(nfs_8b10b_dc_balanced(0x01F), 1);
    /* 0x3FF = 10 ones → not balanced */
    ASSERT_EQ(nfs_8b10b_dc_balanced(0x3FF), 0);
    /* 0x000 = 0 ones → not balanced */
    ASSERT_EQ(nfs_8b10b_dc_balanced(0x000), 0);
}

/* ---- 17. K28.5 comma RD- ---- */

static void test_comma_rdn(void) {
    ASSERT_EQ(nfs_is_comma(NFS_K28_5_RDN), 1);
}

/* ---- 18. K28.5 comma RD+ ---- */

static void test_comma_rdp(void) {
    ASSERT_EQ(nfs_is_comma(NFS_K28_5_RDP), 1);
}

/* ---- 19. Non-comma rejection ---- */

static void test_non_comma(void) {
    ASSERT_EQ(nfs_is_comma(0x155), 0);
    ASSERT_EQ(nfs_is_comma(0x000), 0);
    ASSERT_EQ(nfs_is_comma(0x3FF), 0);
    ASSERT_EQ(nfs_is_comma(0x0FB), 0); /* off by one from K28.5 RD- */
}

/* ---- 20. Wire efficiency min payload 46B ---- */

static void test_efficiency_min(void) {
    /* 46 / (46 + 38) = 46/84 = 0.547619... */
    ASSERT_NEAR(nfs_wire_efficiency(46), 0.547619, 0.001);
}

/* ---- 21. Wire efficiency max payload 1500B ---- */

static void test_efficiency_max(void) {
    /* 1500 / (1500 + 38) = 1500/1538 = 0.975293... */
    ASSERT_NEAR(nfs_wire_efficiency(1500), 0.975293, 0.001);
}

/* ---- 22. Min frame size ---- */

static void test_min_frame_size(void) {
    ASSERT_EQ(nfs_min_frame_size(), 64);
}

/* ---- 23. Max frame size ---- */

static void test_max_frame_size(void) {
    ASSERT_EQ(nfs_max_frame_size(), 1518);
}

/* ---- 24. Frame time 64B at 100 Mbps ---- */

static void test_frame_time_64_100mbps(void) {
    /* (64 + 8 + 12) * 8 * 10 = 84 * 80 = 6720 ns */
    ASSERT_EQ(nfs_frame_time_ns(64, 100), 6720);
}

/* ---- 25. Frame time 1518B at 1 Gbps ---- */

static void test_frame_time_1518_1gbps(void) {
    /* (1518 + 8 + 12) * 8 * 1 = 1538 * 8 = 12304 ns */
    ASSERT_EQ(nfs_frame_time_ns(1518, 1000), 12304);
}

int main(void) {
    printf("Wire Tests\n");

    test_preamble_generate();
    test_preamble_detect();
    test_preamble_detect_noise_prefix();
    test_preamble_not_found();
    test_preamble_buffer_too_small();
    test_ifg_bytes();
    test_ifg_ns_10mbps();
    test_ifg_ns_100mbps();
    test_ifg_ns_1gbps();
    test_bit_time_10mbps();
    test_bit_time_100mbps();
    test_bit_time_1gbps();
    test_rd_balanced();
    test_rd_more_ones();
    test_rd_fewer_ones();
    test_dc_balanced();
    test_comma_rdn();
    test_comma_rdp();
    test_non_comma();
    test_efficiency_min();
    test_efficiency_max();
    test_min_frame_size();
    test_max_frame_size();
    test_frame_time_64_100mbps();
    test_frame_time_1518_1gbps();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
