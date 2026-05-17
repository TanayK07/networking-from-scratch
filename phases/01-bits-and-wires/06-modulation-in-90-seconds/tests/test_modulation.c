/* Unit tests for digital modulation: OOK, BPSK, QPSK, 16-QAM. */

#include "../modulation.h"
#include <math.h>
#include <stdio.h>

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
        if (fabs(_got - _exp) > (tol)) {                                                           \
            fprintf(stderr, "  FAIL %s:%d: %s == %.10f, want %.10f (tol %.1e)\n", __FILE__,        \
                    __LINE__, #expr, _got, _exp, (double)(tol));                                   \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

/* Helper: count bits that differ between two integers */
static int hamming_distance(int a, int b) {
    int x = a ^ b;
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

/* ---- Test 1: OOK modulate/demodulate roundtrip ---- */

static void test_ook_roundtrip(void) {
    for (int bit = 0; bit <= 1; bit++) {
        nfs_symbol_t s = nfs_ook_modulate(bit);
        int dec = nfs_ook_demodulate(s);
        ASSERT_EQ(dec, bit);
    }
}

/* ---- Test 2: OOK symbol values ---- */

static void test_ook_symbol_values(void) {
    nfs_symbol_t s0 = nfs_ook_modulate(0);
    ASSERT_NEAR(s0.i, 0.0, 1e-9);
    ASSERT_NEAR(s0.q, 0.0, 1e-9);

    nfs_symbol_t s1 = nfs_ook_modulate(1);
    ASSERT_NEAR(s1.i, 1.0, 1e-9);
    ASSERT_NEAR(s1.q, 0.0, 1e-9);
}

/* ---- Test 3: BPSK modulate/demodulate roundtrip ---- */

static void test_bpsk_roundtrip(void) {
    for (int bit = 0; bit <= 1; bit++) {
        nfs_symbol_t s = nfs_bpsk_modulate(bit);
        int dec = nfs_bpsk_demodulate(s);
        ASSERT_EQ(dec, bit);
    }
}

/* ---- Test 4: BPSK symbol values ---- */

static void test_bpsk_symbol_values(void) {
    nfs_symbol_t s0 = nfs_bpsk_modulate(0);
    ASSERT_NEAR(s0.i, +1.0, 1e-9);
    ASSERT_NEAR(s0.q, 0.0, 1e-9);

    nfs_symbol_t s1 = nfs_bpsk_modulate(1);
    ASSERT_NEAR(s1.i, -1.0, 1e-9);
    ASSERT_NEAR(s1.q, 0.0, 1e-9);
}

/* ---- Test 5: BPSK noise margin ---- */

static void test_bpsk_noise_margin(void) {
    /* bit 0 -> (+1, 0): adding noise of +/-0.3 should still demodulate to 0 */
    nfs_symbol_t s0_noisy1 = {1.0 + 0.3, 0.0};
    ASSERT_EQ(nfs_bpsk_demodulate(s0_noisy1), 0);

    nfs_symbol_t s0_noisy2 = {1.0 - 0.3, 0.0};
    ASSERT_EQ(nfs_bpsk_demodulate(s0_noisy2), 0);

    /* bit 1 -> (-1, 0): adding noise of +/-0.3 should still demodulate to 1 */
    nfs_symbol_t s1_noisy1 = {-1.0 + 0.3, 0.0};
    ASSERT_EQ(nfs_bpsk_demodulate(s1_noisy1), 1);

    nfs_symbol_t s1_noisy2 = {-1.0 - 0.3, 0.0};
    ASSERT_EQ(nfs_bpsk_demodulate(s1_noisy2), 1);
}

/* ---- Test 6: QPSK all 4 dibits roundtrip ---- */

static void test_qpsk_roundtrip(void) {
    for (int d = 0; d < 4; d++) {
        nfs_symbol_t s = nfs_qpsk_modulate(d);
        int dec = nfs_qpsk_demodulate(s);
        ASSERT_EQ(dec, d);
    }
}

/* ---- Test 7: QPSK constellation coordinates ---- */

static void test_qpsk_coordinates(void) {
    const double inv_sqrt2 = 0.70710678118654752;

    /* 00 -> 45 deg -> (+1/sqrt2, +1/sqrt2) */
    nfs_symbol_t s0 = nfs_qpsk_modulate(0);
    ASSERT_NEAR(s0.i, +inv_sqrt2, 1e-9);
    ASSERT_NEAR(s0.q, +inv_sqrt2, 1e-9);

    /* 01 -> 135 deg -> (-1/sqrt2, +1/sqrt2) */
    nfs_symbol_t s1 = nfs_qpsk_modulate(1);
    ASSERT_NEAR(s1.i, -inv_sqrt2, 1e-9);
    ASSERT_NEAR(s1.q, +inv_sqrt2, 1e-9);

    /* 11 -> 225 deg -> (-1/sqrt2, -1/sqrt2) */
    nfs_symbol_t s3 = nfs_qpsk_modulate(3);
    ASSERT_NEAR(s3.i, -inv_sqrt2, 1e-9);
    ASSERT_NEAR(s3.q, -inv_sqrt2, 1e-9);

    /* 10 -> 315 deg -> (+1/sqrt2, -1/sqrt2) */
    nfs_symbol_t s2 = nfs_qpsk_modulate(2);
    ASSERT_NEAR(s2.i, +inv_sqrt2, 1e-9);
    ASSERT_NEAR(s2.q, -inv_sqrt2, 1e-9);
}

/* ---- Test 8: QPSK Gray coding ---- */

static void test_qpsk_gray_coding(void) {
    /* Adjacent constellation points (by phase angle) should differ by 1 bit.
     * Order by phase: 00 (45), 01 (135), 11 (225), 10 (315).
     * Adjacent pairs: (00,01), (01,11), (11,10), (10,00) wrap-around. */
    int order[] = {0, 1, 3, 2};
    for (int i = 0; i < 4; i++) {
        int next = (i + 1) % 4;
        int dist = hamming_distance(order[i], order[next]);
        ASSERT_EQ(dist, 1);
    }
}

/* ---- Test 9: 16-QAM all 16 nibbles roundtrip ---- */

static void test_qam16_roundtrip(void) {
    for (int n = 0; n < 16; n++) {
        nfs_symbol_t s = nfs_qam16_modulate(n);
        int dec = nfs_qam16_demodulate(s);
        ASSERT_EQ(dec, n);
    }
}

/* ---- Test 10: 16-QAM known coordinates ---- */

static void test_qam16_known_coordinates(void) {
    /* nibble 0b0000 (0) -> I: 00->-3, Q: 00->-3 -> (-3, -3) */
    nfs_symbol_t s0 = nfs_qam16_modulate(0x0);
    ASSERT_NEAR(s0.i, -3.0, 1e-9);
    ASSERT_NEAR(s0.q, -3.0, 1e-9);

    /* nibble 0b0101 (5) -> I: 01->-1, Q: 01->-1 -> (-1, -1) */
    nfs_symbol_t s5 = nfs_qam16_modulate(0x5);
    ASSERT_NEAR(s5.i, -1.0, 1e-9);
    ASSERT_NEAR(s5.q, -1.0, 1e-9);

    /* nibble 0b1111 (15) -> I: 11->+1, Q: 11->+1 -> (+1, +1) */
    nfs_symbol_t sf = nfs_qam16_modulate(0xF);
    ASSERT_NEAR(sf.i, +1.0, 1e-9);
    ASSERT_NEAR(sf.q, +1.0, 1e-9);

    /* nibble 0b1010 (10) -> I: 10->+3, Q: 10->+3 -> (+3, +3) */
    nfs_symbol_t sa = nfs_qam16_modulate(0xA);
    ASSERT_NEAR(sa.i, +3.0, 1e-9);
    ASSERT_NEAR(sa.q, +3.0, 1e-9);
}

/* ---- Test 11: 16-QAM Gray coding for I axis ---- */

static void test_qam16_gray_coding(void) {
    /* Gray coding: adjacent I values differ by 1 bit in upper 2 bits.
     * I levels in order: -3 (00), -1 (01), +1 (11), +3 (10).
     * Adjacent pairs: (00,01), (01,11), (11,10). Each should have Hamming dist 1. */
    int gray_codes[] = {0, 1, 3, 2}; /* 00, 01, 11, 10 */
    for (int i = 0; i < 3; i++) {
        int dist = hamming_distance(gray_codes[i], gray_codes[i + 1]);
        ASSERT_EQ(dist, 1);
    }

    /* Also verify wrap-around: 10 and 00 differ by 1 bit */
    ASSERT_EQ(hamming_distance(gray_codes[3], gray_codes[0]), 1);

    /* Verify the actual nibble mappings reflect this:
     * nibble with I=00 (bits b3b2=00): e.g. 0b0000 -> I=-3
     * nibble with I=01 (bits b3b2=01): e.g. 0b0100 -> I=-1
     * nibble with I=11 (bits b3b2=11): e.g. 0b1100 -> I=+1
     * nibble with I=10 (bits b3b2=10): e.g. 0b1000 -> I=+3 */
    nfs_symbol_t s_i00 = nfs_qam16_modulate(0x0);
    nfs_symbol_t s_i01 = nfs_qam16_modulate(0x4);
    nfs_symbol_t s_i11 = nfs_qam16_modulate(0xC);
    nfs_symbol_t s_i10 = nfs_qam16_modulate(0x8);

    ASSERT_NEAR(s_i00.i, -3.0, 1e-9);
    ASSERT_NEAR(s_i01.i, -1.0, 1e-9);
    ASSERT_NEAR(s_i11.i, +1.0, 1e-9);
    ASSERT_NEAR(s_i10.i, +3.0, 1e-9);
}

/* ---- Test 12: bits_per_symbol ---- */

static void test_bits_per_symbol(void) {
    ASSERT_EQ(nfs_bits_per_symbol(NFS_MOD_OOK), 1);
    ASSERT_EQ(nfs_bits_per_symbol(NFS_MOD_BPSK), 1);
    ASSERT_EQ(nfs_bits_per_symbol(NFS_MOD_QPSK), 2);
    ASSERT_EQ(nfs_bits_per_symbol(NFS_MOD_QAM16), 4);
}

/* ---- Test 13: bitrate calculation ---- */

static void test_bitrate(void) {
    ASSERT_NEAR(nfs_bitrate(1000.0, NFS_MOD_QPSK), 2000.0, 1e-9);
    ASSERT_NEAR(nfs_bitrate(1000.0, NFS_MOD_QAM16), 4000.0, 1e-9);
    ASSERT_NEAR(nfs_bitrate(1000.0, NFS_MOD_OOK), 1000.0, 1e-9);
    ASSERT_NEAR(nfs_bitrate(1000.0, NFS_MOD_BPSK), 1000.0, 1e-9);
}

/* ---- Test 14: Invalid inputs ---- */

static void test_invalid_inputs(void) {
    /* QPSK: dibit > 3 should produce zero symbol */
    nfs_symbol_t s_bad_qpsk = nfs_qpsk_modulate(4);
    ASSERT_NEAR(s_bad_qpsk.i, 0.0, 1e-9);
    ASSERT_NEAR(s_bad_qpsk.q, 0.0, 1e-9);

    nfs_symbol_t s_bad_qpsk2 = nfs_qpsk_modulate(-1);
    ASSERT_NEAR(s_bad_qpsk2.i, 0.0, 1e-9);
    ASSERT_NEAR(s_bad_qpsk2.q, 0.0, 1e-9);

    /* 16-QAM: nibble > 15 should produce zero symbol */
    nfs_symbol_t s_bad_qam = nfs_qam16_modulate(16);
    ASSERT_NEAR(s_bad_qam.i, 0.0, 1e-9);
    ASSERT_NEAR(s_bad_qam.q, 0.0, 1e-9);

    nfs_symbol_t s_bad_qam2 = nfs_qam16_modulate(-1);
    ASSERT_NEAR(s_bad_qam2.i, 0.0, 1e-9);
    ASSERT_NEAR(s_bad_qam2.q, 0.0, 1e-9);

    /* bits_per_symbol for invalid scheme */
    ASSERT_EQ(nfs_bits_per_symbol((nfs_mod_scheme_t)99), -1);

    /* bitrate for invalid scheme */
    ASSERT_NEAR(nfs_bitrate(1000.0, (nfs_mod_scheme_t)99), -1.0, 1e-9);
}

/* ---- Additional: QPSK unit circle verification ---- */

static void test_qpsk_unit_circle(void) {
    /* All QPSK symbols should lie on the unit circle: |s| = 1 */
    for (int d = 0; d < 4; d++) {
        nfs_symbol_t s = nfs_qpsk_modulate(d);
        double magnitude = sqrt(s.i * s.i + s.q * s.q);
        ASSERT_NEAR(magnitude, 1.0, 1e-9);
    }
}

/* ---- Additional: 16-QAM coordinate grid verification ---- */

static void test_qam16_grid(void) {
    /* Every 16-QAM symbol should have I and Q from {-3, -1, +1, +3} */
    for (int n = 0; n < 16; n++) {
        nfs_symbol_t s = nfs_qam16_modulate(n);
        int i_valid = (fabs(s.i - (-3.0)) < 1e-9) || (fabs(s.i - (-1.0)) < 1e-9) ||
                      (fabs(s.i - (+1.0)) < 1e-9) || (fabs(s.i - (+3.0)) < 1e-9);
        int q_valid = (fabs(s.q - (-3.0)) < 1e-9) || (fabs(s.q - (-1.0)) < 1e-9) ||
                      (fabs(s.q - (+1.0)) < 1e-9) || (fabs(s.q - (+3.0)) < 1e-9);
        ASSERT_TRUE(i_valid);
        ASSERT_TRUE(q_valid);
    }
}

/* ---- Additional: OOK demodulate at threshold boundary ---- */

static void test_ook_threshold(void) {
    /* Amplitude exactly 0.5 should demodulate to 1 (>= 0.5) */
    nfs_symbol_t at_thresh = {0.5, 0.0};
    ASSERT_EQ(nfs_ook_demodulate(at_thresh), 1);

    /* Amplitude just below 0.5 should demodulate to 0 */
    nfs_symbol_t below = {0.49, 0.0};
    ASSERT_EQ(nfs_ook_demodulate(below), 0);
}

int main(void) {
    printf("Modulation Tests\n");

    test_ook_roundtrip();
    test_ook_symbol_values();
    test_bpsk_roundtrip();
    test_bpsk_symbol_values();
    test_bpsk_noise_margin();
    test_qpsk_roundtrip();
    test_qpsk_coordinates();
    test_qpsk_gray_coding();
    test_qam16_roundtrip();
    test_qam16_known_coordinates();
    test_qam16_gray_coding();
    test_bits_per_symbol();
    test_bitrate();
    test_invalid_inputs();
    test_qpsk_unit_circle();
    test_qam16_grid();
    test_ook_threshold();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
