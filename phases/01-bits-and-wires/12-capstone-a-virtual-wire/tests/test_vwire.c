/* Comprehensive unit tests for virtual wire simulator. */

#include "../vwire.h"
#include <math.h>
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

/* ================================================================
 * 1. Basic config/init tests
 * ================================================================ */

/* Test 1: Init with zero config */
static void test_init_zero_config(void) {
    printf("  test_init_zero_config...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 1;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    ASSERT_NEAR(w.cfg.ber, 0.0, 1e-15);
    ASSERT_NEAR(w.cfg.drop_prob, 0.0, 1e-15);
    ASSERT_EQ(w.cfg.delay_us, 0);
    ASSERT_EQ(w.cfg.jitter_us, 0);
    ASSERT_NEAR(w.cfg.reorder_prob, 0.0, 1e-15);
    ASSERT_EQ(w.tx_frames, 0);
    ASSERT_EQ(w.rx_frames, 0);
    ASSERT_EQ(w.dropped, 0);
    ASSERT_EQ(w.bit_errors, 0);
    ASSERT_EQ(w.reordered, 0);
}

/* Test 2: Init preserves config */
static void test_init_preserves_config(void) {
    printf("  test_init_preserves_config...\n");
    nfs_wire_cfg_t cfg = {.ber = 0.01,
                          .drop_prob = 0.05,
                          .delay_us = 1000,
                          .jitter_us = 200,
                          .reorder_prob = 0.03,
                          .seed = 42};
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    ASSERT_NEAR(w.cfg.ber, 0.01, 1e-15);
    ASSERT_NEAR(w.cfg.drop_prob, 0.05, 1e-15);
    ASSERT_EQ(w.cfg.delay_us, 1000);
    ASSERT_EQ(w.cfg.jitter_us, 200);
    ASSERT_NEAR(w.cfg.reorder_prob, 0.03, 1e-15);
    ASSERT_EQ(w.cfg.seed, 42);
}

/* ================================================================
 * 2. BER tests (seeded for determinism)
 * ================================================================ */

/* Test 3: BER=0, no bits flipped */
static void test_ber_zero(void) {
    printf("  test_ber_zero...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t frame[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t orig[4];
    memcpy(orig, frame, 4);

    size_t flipped = nfs_wire_apply_ber(&w, frame, sizeof(frame));
    ASSERT_EQ(flipped, 0);
    ASSERT_TRUE(memcmp(frame, orig, 4) == 0);
}

/* Test 4: BER=1.0, every bit flipped (bitwise NOT) */
static void test_ber_one(void) {
    printf("  test_ber_one...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.ber = 1.0;
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t frame[] = {0xDE, 0xAD, 0xBE, 0xEF};
    size_t flipped = nfs_wire_apply_ber(&w, frame, sizeof(frame));

    ASSERT_EQ(flipped, 32); /* all 32 bits flipped */
    ASSERT_EQ(frame[0], (uint8_t)~0xDE);
    ASSERT_EQ(frame[1], (uint8_t)~0xAD);
    ASSERT_EQ(frame[2], (uint8_t)~0xBE);
    ASSERT_EQ(frame[3], (uint8_t)~0xEF);
}

/* Test 5: BER=0.5 statistical, 1000 bytes, seed=42 */
static void test_ber_half_statistical(void) {
    printf("  test_ber_half_statistical...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.ber = 0.5;
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t frame[1000];
    memset(frame, 0xAA, sizeof(frame));

    size_t flipped = nfs_wire_apply_ber(&w, frame, sizeof(frame));
    /* Deterministic with seed=42: exactly 4124 bits flipped */
    ASSERT_EQ(flipped, 4124);
    /* Sanity: roughly half of 8000 bits */
    ASSERT_TRUE(flipped > 3500 && flipped < 4500);
}

/* Test 6: BER preserves frame length */
static void test_ber_preserves_length(void) {
    printf("  test_ber_preserves_length...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.ber = 0.1;
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t frame[50];
    memset(frame, 0x55, sizeof(frame));

    uint8_t out[64];
    size_t out_len;
    int rc = nfs_wire_transmit(&w, frame, sizeof(frame), out, sizeof(out), &out_len);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(out_len, 50);
}

/* Test 7: Small BER on small frame, deterministic count */
static void test_small_ber_deterministic(void) {
    printf("  test_small_ber_deterministic...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.ber = 0.01;
    cfg.seed = 7;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t frame[100];
    memset(frame, 0x55, sizeof(frame));

    size_t flipped = nfs_wire_apply_ber(&w, frame, sizeof(frame));
    /* Deterministic with seed=7, BER=0.01, 100 bytes: exactly 15 flips */
    ASSERT_EQ(flipped, 15);
}

/* ================================================================
 * 3. Drop tests
 * ================================================================ */

/* Test 8: drop_prob=0, never drops */
static void test_drop_zero(void) {
    printf("  test_drop_zero...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t frame[] = {0x01};
    uint8_t out[16];
    size_t out_len;

    for (int i = 0; i < 100; i++) {
        int rc = nfs_wire_transmit(&w, frame, 1, out, sizeof(out), &out_len);
        ASSERT_EQ(rc, 0);
    }
    ASSERT_EQ(w.tx_frames, 100);
    ASSERT_EQ(w.rx_frames, 100);
    ASSERT_EQ(w.dropped, 0);
}

/* Test 9: drop_prob=1.0, always drops */
static void test_drop_one(void) {
    printf("  test_drop_one...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.drop_prob = 1.0;
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t frame[] = {0x01};
    uint8_t out[16];
    size_t out_len;

    for (int i = 0; i < 100; i++) {
        int rc = nfs_wire_transmit(&w, frame, 1, out, sizeof(out), &out_len);
        ASSERT_EQ(rc, 1);
    }
    ASSERT_EQ(w.tx_frames, 100);
    ASSERT_EQ(w.rx_frames, 0);
    ASSERT_EQ(w.dropped, 100);
}

/* Test 10: drop_prob=0.5 statistical */
static void test_drop_half_statistical(void) {
    printf("  test_drop_half_statistical...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.drop_prob = 0.5;
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t frame[] = {0x01};
    uint8_t out[16];
    size_t out_len;

    for (int i = 0; i < 10000; i++) {
        nfs_wire_transmit(&w, frame, 1, out, sizeof(out), &out_len);
    }

    ASSERT_EQ(w.tx_frames, 10000);
    /* With seed=42: exactly 5109 drops */
    double drop_rate = (double)w.dropped / (double)w.tx_frames;
    /* Within +/- 5% of 0.5 */
    ASSERT_TRUE(drop_rate > 0.45 && drop_rate < 0.55);
}

/* ================================================================
 * 4. Delay tests
 * ================================================================ */

/* Test 11: Zero delay, zero jitter */
static void test_delay_zero(void) {
    printf("  test_delay_zero...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint32_t d = nfs_wire_delay_us(&w);
    ASSERT_EQ(d, 0);
}

/* Test 12: Fixed delay, no jitter */
static void test_delay_fixed(void) {
    printf("  test_delay_fixed...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.delay_us = 1000;
    cfg.jitter_us = 0;
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    for (int i = 0; i < 10; i++) {
        uint32_t d = nfs_wire_delay_us(&w);
        ASSERT_EQ(d, 1000);
    }
}

/* Test 13: Delay with jitter */
static void test_delay_with_jitter(void) {
    printf("  test_delay_with_jitter...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.delay_us = 1000;
    cfg.jitter_us = 100;
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    int saw_variation = 0;
    uint32_t first = nfs_wire_delay_us(&w);
    for (int i = 0; i < 99; i++) {
        uint32_t d = nfs_wire_delay_us(&w);
        if (d != first)
            saw_variation = 1;
        /* All values should be >= 0 (uint32_t guarantees this,
         * but the clamping in the implementation ensures no underflow) */
    }
    ASSERT_TRUE(saw_variation);
}

/* ================================================================
 * 5. Reorder tests
 * ================================================================ */

/* Test 14: reorder_prob=0 */
static void test_reorder_zero(void) {
    printf("  test_reorder_zero...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(nfs_wire_should_reorder(&w), 0);
    }
}

/* Test 15: reorder_prob=1.0 */
static void test_reorder_one(void) {
    printf("  test_reorder_one...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.reorder_prob = 1.0;
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(nfs_wire_should_reorder(&w), 1);
    }
}

/* ================================================================
 * 6. Transmit integration tests
 * ================================================================ */

/* Test 16: Clean wire, frame passes unchanged */
static void test_transmit_clean(void) {
    printf("  test_transmit_clean...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t frame[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t out[64];
    size_t out_len;

    int rc = nfs_wire_transmit(&w, frame, sizeof(frame), out, sizeof(out), &out_len);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(out_len, sizeof(frame));
    ASSERT_TRUE(memcmp(frame, out, sizeof(frame)) == 0);
}

/* Test 17: Drop-only wire, no corruption */
static void test_transmit_drop_only(void) {
    printf("  test_transmit_drop_only...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.drop_prob = 0.5;
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t frame[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t out[64];
    size_t out_len;

    int delivered = 0;
    for (int i = 0; i < 100; i++) {
        int rc = nfs_wire_transmit(&w, frame, sizeof(frame), out, sizeof(out), &out_len);
        if (rc == 0) {
            /* Delivered frame must match original (no BER) */
            ASSERT_EQ(out_len, sizeof(frame));
            ASSERT_TRUE(memcmp(frame, out, sizeof(frame)) == 0);
            delivered++;
        }
    }
    /* Some delivered, some dropped */
    ASSERT_TRUE(delivered > 0);
    ASSERT_TRUE(delivered < 100);
}

/* Test 18: BER-only wire, delivered but possibly corrupted */
static void test_transmit_ber_only(void) {
    printf("  test_transmit_ber_only...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.ber = 0.1;
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t frame[] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t out[64];
    size_t out_len;

    int rc = nfs_wire_transmit(&w, frame, sizeof(frame), out, sizeof(out), &out_len);
    ASSERT_EQ(rc, 0); /* always delivered (no drops) */
    ASSERT_EQ(out_len, sizeof(frame));
    ASSERT_TRUE(w.bit_errors > 0); /* with BER=0.1, very likely some errors */
}

/* Test 19: Full stats tracking */
static void test_transmit_stats_tracking(void) {
    printf("  test_transmit_stats_tracking...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.drop_prob = 1.0;
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t frame[] = {0x01};
    uint8_t out[16];
    size_t out_len;

    /* All dropped */
    for (int i = 0; i < 5; i++) {
        nfs_wire_transmit(&w, frame, 1, out, sizeof(out), &out_len);
    }
    ASSERT_EQ(w.tx_frames, 5);
    ASSERT_EQ(w.rx_frames, 0);
    ASSERT_EQ(w.dropped, 5);

    /* Reset and use clean wire */
    nfs_wire_reset_stats(&w);
    w.cfg.drop_prob = 0.0;
    ASSERT_EQ(w.tx_frames, 0);
    ASSERT_EQ(w.rx_frames, 0);
    ASSERT_EQ(w.dropped, 0);

    for (int i = 0; i < 3; i++) {
        nfs_wire_transmit(&w, frame, 1, out, sizeof(out), &out_len);
    }
    ASSERT_EQ(w.tx_frames, 3);
    ASSERT_EQ(w.rx_frames, 3);
    ASSERT_EQ(w.dropped, 0);
}

/* ================================================================
 * 7. PRNG tests
 * ================================================================ */

/* Test 20: Deterministic — same seed, same sequence */
static void test_prng_deterministic(void) {
    printf("  test_prng_deterministic...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 42;
    nfs_wire_t w1, w2;
    nfs_wire_init(&w1, &cfg);
    nfs_wire_init(&w2, &cfg);

    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(nfs_wire_rand(&w1), nfs_wire_rand(&w2));
    }
}

/* Test 21: Different seeds produce different sequences */
static void test_prng_different_seeds(void) {
    printf("  test_prng_different_seeds...\n");
    nfs_wire_cfg_t cfg1 = {0};
    cfg1.seed = 42;
    nfs_wire_cfg_t cfg2 = {0};
    cfg2.seed = 99;

    nfs_wire_t w1, w2;
    nfs_wire_init(&w1, &cfg1);
    nfs_wire_init(&w2, &cfg2);

    int differ = 0;
    for (int i = 0; i < 10; i++) {
        if (nfs_wire_rand(&w1) != nfs_wire_rand(&w2))
            differ = 1;
    }
    ASSERT_TRUE(differ);
}

/* Test 22: xorshift32 known value with seed=1 */
static void test_prng_known_value(void) {
    printf("  test_prng_known_value...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 1;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    /*
     * xorshift32 with seed=1:
     *   state = 1
     *   state ^= 1 << 13 = 1 ^ 8192 = 8193
     *   state ^= 8193 >> 17 = 8193 ^ 0 = 8193
     *   state ^= 8193 << 5 = 8193 ^ 262176 = 270369
     *   first output = 270369
     */
    uint32_t v = nfs_wire_rand(&w);
    ASSERT_EQ(v, 270369);
}

/* Test 23: rand_double range [0.0, 1.0) */
static void test_prng_double_range(void) {
    printf("  test_prng_double_range...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    for (int i = 0; i < 1000; i++) {
        double d = nfs_wire_rand_double(&w);
        ASSERT_TRUE(d >= 0.0);
        ASSERT_TRUE(d < 1.0);
    }
}

/* ================================================================
 * 8. Edge cases
 * ================================================================ */

/* Test 24: NULL frame pointer returns -1 */
static void test_null_frame(void) {
    printf("  test_null_frame...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t out[16];
    size_t out_len;

    int rc = nfs_wire_transmit(&w, NULL, 10, out, sizeof(out), &out_len);
    ASSERT_EQ(rc, -1);
}

/* Test 25: Zero-length frame, valid, no BER applied */
static void test_zero_length_frame(void) {
    printf("  test_zero_length_frame...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.ber = 0.5;
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    uint8_t out[16];
    size_t out_len;

    int rc = nfs_wire_transmit(&w, (const uint8_t *)"", 0, out, sizeof(out), &out_len);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(out_len, 0);
    ASSERT_EQ(w.bit_errors, 0);
    ASSERT_EQ(w.tx_frames, 1);
    ASSERT_EQ(w.rx_frames, 1);
}

/* Test 26: Very large frame (65535 bytes), clean wire */
static void test_large_frame(void) {
    printf("  test_large_frame...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    size_t sz = 65535;
    uint8_t *frame = (uint8_t *)malloc(sz);
    uint8_t *out = (uint8_t *)malloc(sz);
    ASSERT_TRUE(frame != NULL);
    ASSERT_TRUE(out != NULL);

    memset(frame, 0xAB, sz);
    size_t out_len;

    int rc = nfs_wire_transmit(&w, frame, sz, out, sz, &out_len);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(out_len, sz);
    ASSERT_TRUE(memcmp(frame, out, sz) == 0);

    free(frame);
    free(out);
}

/* ================================================================
 * Additional: loss rate and stats string
 * ================================================================ */

static void test_loss_rate(void) {
    printf("  test_loss_rate...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    /* No frames sent: loss rate is 0 */
    ASSERT_NEAR(nfs_wire_loss_rate(&w), 0.0, 1e-15);

    /* Simulate some dropped frames manually */
    w.tx_frames = 100;
    w.dropped = 25;
    ASSERT_NEAR(nfs_wire_loss_rate(&w), 0.25, 1e-10);
}

static void test_stats_string(void) {
    printf("  test_stats_string...\n");
    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 42;
    nfs_wire_t w;
    nfs_wire_init(&w, &cfg);

    w.tx_frames = 100;
    w.rx_frames = 95;
    w.dropped = 5;
    w.bit_errors = 42;
    w.reordered = 3;

    char buf[256];
    nfs_wire_stats_string(&w, buf, sizeof(buf));

    /* Verify the string contains key numbers */
    ASSERT_TRUE(strstr(buf, "100") != NULL);
    ASSERT_TRUE(strstr(buf, "95") != NULL);
    ASSERT_TRUE(strstr(buf, "42") != NULL);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("=== Virtual Wire Tests ===\n");

    /* Basic config/init */
    test_init_zero_config();
    test_init_preserves_config();

    /* BER tests */
    test_ber_zero();
    test_ber_one();
    test_ber_half_statistical();
    test_ber_preserves_length();
    test_small_ber_deterministic();

    /* Drop tests */
    test_drop_zero();
    test_drop_one();
    test_drop_half_statistical();

    /* Delay tests */
    test_delay_zero();
    test_delay_fixed();
    test_delay_with_jitter();

    /* Reorder tests */
    test_reorder_zero();
    test_reorder_one();

    /* Transmit integration */
    test_transmit_clean();
    test_transmit_drop_only();
    test_transmit_ber_only();
    test_transmit_stats_tracking();

    /* PRNG tests */
    test_prng_deterministic();
    test_prng_different_seeds();
    test_prng_known_value();
    test_prng_double_range();

    /* Edge cases */
    test_null_frame();
    test_zero_length_frame();
    test_large_frame();

    /* Stats */
    test_loss_rate();
    test_stats_string();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
