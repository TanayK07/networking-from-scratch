/* Unit tests for Hamming(7,4) forward error correction. */

#include "../hamming.h"
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

/* ---- Encode all 16 values, verify syndrome is 0 ---- */

static void test_encode_all_valid_syndrome(void) {
    for (uint8_t i = 0; i < 16; i++) {
        uint8_t cw = nfs_hamming74_encode(i);
        ASSERT_EQ(nfs_hamming74_syndrome(cw), 0);
    }
}

/* ---- Decode all 16 values without error ---- */

static void test_decode_all_no_error(void) {
    for (uint8_t i = 0; i < 16; i++) {
        uint8_t cw = nfs_hamming74_encode(i);
        uint8_t recovered;
        int result = nfs_hamming74_decode(cw, &recovered);
        ASSERT_EQ(result, 0);
        ASSERT_EQ(recovered, i);
    }
}

/* ---- Single bit flip correction for data=0 ---- */

static void test_correct_single_flip_data0(void) {
    uint8_t cw = nfs_hamming74_encode(0);
    for (int pos = 1; pos <= 7; pos++) {
        uint8_t corrupted = cw ^ (uint8_t)(1 << (pos - 1));
        uint8_t recovered;
        int result = nfs_hamming74_decode(corrupted, &recovered);
        ASSERT_EQ(result, 1);
        ASSERT_EQ(recovered, 0);
    }
}

/* ---- Single bit flip correction for data=0xF ---- */

static void test_correct_single_flip_dataF(void) {
    uint8_t cw = nfs_hamming74_encode(0xF);
    for (int pos = 1; pos <= 7; pos++) {
        uint8_t corrupted = cw ^ (uint8_t)(1 << (pos - 1));
        uint8_t recovered;
        int result = nfs_hamming74_decode(corrupted, &recovered);
        ASSERT_EQ(result, 1);
        ASSERT_EQ(recovered, 0xF);
    }
}

/* ---- Single bit flip correction for data=0xA (1010) ---- */

static void test_correct_single_flip_dataA(void) {
    uint8_t cw = nfs_hamming74_encode(0xA);
    for (int pos = 1; pos <= 7; pos++) {
        uint8_t corrupted = cw ^ (uint8_t)(1 << (pos - 1));
        uint8_t recovered;
        int result = nfs_hamming74_decode(corrupted, &recovered);
        ASSERT_EQ(result, 1);
        ASSERT_EQ(recovered, 0xA);
    }
}

/* ---- Single bit flip correction for data=0x5 (0101) ---- */

static void test_correct_single_flip_data5(void) {
    uint8_t cw = nfs_hamming74_encode(0x5);
    for (int pos = 1; pos <= 7; pos++) {
        uint8_t corrupted = cw ^ (uint8_t)(1 << (pos - 1));
        uint8_t recovered;
        int result = nfs_hamming74_decode(corrupted, &recovered);
        ASSERT_EQ(result, 1);
        ASSERT_EQ(recovered, 0x5);
    }
}

/* ---- Comprehensive: all 16 data values x all 7 bit positions ---- */

static void test_correct_all_single_flips(void) {
    for (uint8_t d = 0; d < 16; d++) {
        uint8_t cw = nfs_hamming74_encode(d);
        for (int pos = 1; pos <= 7; pos++) {
            uint8_t corrupted = cw ^ (uint8_t)(1 << (pos - 1));
            uint8_t recovered;
            int result = nfs_hamming74_decode(corrupted, &recovered);
            ASSERT_EQ(result, 1);
            ASSERT_EQ(recovered, d);
        }
    }
}

/* ---- Syndrome identifies correct error position ---- */

static void test_syndrome_position(void) {
    uint8_t cw = nfs_hamming74_encode(0x7); /* 0111 */
    for (int pos = 1; pos <= 7; pos++) {
        uint8_t corrupted = cw ^ (uint8_t)(1 << (pos - 1));
        uint8_t syn = nfs_hamming74_syndrome(corrupted);
        ASSERT_EQ(syn, pos);
    }
}

/* ---- Known codeword values ---- */

static void test_known_codewords(void) {
    /* data=0 (0000): all parities should be 0 → codeword 0000000 */
    ASSERT_EQ(nfs_hamming74_encode(0), 0x00);

    /* data=0xF (1111):
     * d1=1, d2=1, d3=1, d4=1
     * p1 = d1^d2^d4 = 1^1^1 = 1
     * p2 = d1^d3^d4 = 1^1^1 = 1
     * p3 = d2^d3^d4 = 1^1^1 = 1
     * Codeword: pos1=1 pos2=1 pos3=1 pos4=1 pos5=1 pos6=1 pos7=1
     * = 1111111 = 0x7F */
    ASSERT_EQ(nfs_hamming74_encode(0xF), 0x7F);

    /* data=0x8 (1000):
     * d1=1, d2=0, d3=0, d4=0
     * p1 = 1^0^0 = 1
     * p2 = 1^0^0 = 1
     * p3 = 0^0^0 = 0
     * Codeword: p1=1 p2=1 d1=1 p3=0 d2=0 d3=0 d4=0
     * = 0000111 = 0x07 */
    ASSERT_EQ(nfs_hamming74_encode(0x8), 0x07);
}

/* ---- Encode only uses lower nibble ---- */

static void test_encode_ignores_upper_nibble(void) {
    /* 0xF5 & 0x0F = 0x05, should encode same as 0x05 */
    uint8_t cw1 = nfs_hamming74_encode(0xF5);
    uint8_t cw2 = nfs_hamming74_encode(0x05);
    ASSERT_EQ(cw1, cw2);
}

/* ---- Buffer encode/decode roundtrip ---- */

static void test_buf_roundtrip_simple(void) {
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t codes[8];
    uint8_t decoded[4];

    size_t ncodes = nfs_hamming_encode_buf(data, 4, codes, sizeof(codes));
    ASSERT_EQ(ncodes, 8);

    int n = nfs_hamming_decode_buf(codes, ncodes, decoded, sizeof(decoded));
    ASSERT_EQ(n, 4);
    ASSERT_EQ(decoded[0], 0xDE);
    ASSERT_EQ(decoded[1], 0xAD);
    ASSERT_EQ(decoded[2], 0xBE);
    ASSERT_EQ(decoded[3], 0xEF);
}

static void test_buf_roundtrip_all_zeros(void) {
    uint8_t data[] = {0x00, 0x00};
    uint8_t codes[4];
    uint8_t decoded[2];

    size_t ncodes = nfs_hamming_encode_buf(data, 2, codes, sizeof(codes));
    ASSERT_EQ(ncodes, 4);

    int n = nfs_hamming_decode_buf(codes, ncodes, decoded, sizeof(decoded));
    ASSERT_EQ(n, 2);
    ASSERT_EQ(decoded[0], 0x00);
    ASSERT_EQ(decoded[1], 0x00);
}

static void test_buf_roundtrip_all_ff(void) {
    uint8_t data[] = {0xFF, 0xFF};
    uint8_t codes[4];
    uint8_t decoded[2];

    size_t ncodes = nfs_hamming_encode_buf(data, 2, codes, sizeof(codes));
    ASSERT_EQ(ncodes, 4);

    int n = nfs_hamming_decode_buf(codes, ncodes, decoded, sizeof(decoded));
    ASSERT_EQ(n, 2);
    ASSERT_EQ(decoded[0], 0xFF);
    ASSERT_EQ(decoded[1], 0xFF);
}

static void test_buf_single_byte(void) {
    uint8_t data[] = {0x42};
    uint8_t codes[2];
    uint8_t decoded[1];

    size_t ncodes = nfs_hamming_encode_buf(data, 1, codes, sizeof(codes));
    ASSERT_EQ(ncodes, 2);

    int n = nfs_hamming_decode_buf(codes, ncodes, decoded, sizeof(decoded));
    ASSERT_EQ(n, 1);
    ASSERT_EQ(decoded[0], 0x42);
}

static void test_buf_with_correction(void) {
    uint8_t data[] = {0xAB};
    uint8_t codes[2];
    uint8_t decoded[1];

    size_t ncodes = nfs_hamming_encode_buf(data, 1, codes, sizeof(codes));
    ASSERT_EQ(ncodes, 2);

    /* Corrupt one bit in each codeword */
    codes[0] ^= 0x01; /* flip bit 0 (position 1) */
    codes[1] ^= 0x04; /* flip bit 2 (position 3) */

    int n = nfs_hamming_decode_buf(codes, ncodes, decoded, sizeof(decoded));
    ASSERT_EQ(n, 1);
    ASSERT_EQ(decoded[0], 0xAB);
}

static void test_buf_odd_code_len(void) {
    uint8_t codes[3] = {0};
    uint8_t decoded[2];
    int n = nfs_hamming_decode_buf(codes, 3, decoded, sizeof(decoded));
    ASSERT_EQ(n, -1);
}

static void test_buf_output_too_small(void) {
    uint8_t data[] = {0xAA, 0xBB};
    uint8_t codes[2]; /* too small for 4 codewords */
    size_t ncodes = nfs_hamming_encode_buf(data, 2, codes, sizeof(codes));
    ASSERT_EQ(ncodes, 0);
}

/* ---- Codeword stays in 7 bits ---- */

static void test_codeword_7bit_range(void) {
    for (uint8_t i = 0; i < 16; i++) {
        uint8_t cw = nfs_hamming74_encode(i);
        ASSERT_TRUE(cw <= 0x7F);
    }
}

int main(void) {
    printf("Hamming(7,4) Tests\n");

    test_encode_all_valid_syndrome();
    test_decode_all_no_error();
    test_correct_single_flip_data0();
    test_correct_single_flip_dataF();
    test_correct_single_flip_dataA();
    test_correct_single_flip_data5();
    test_correct_all_single_flips();
    test_syndrome_position();
    test_known_codewords();
    test_encode_ignores_upper_nibble();
    test_buf_roundtrip_simple();
    test_buf_roundtrip_all_zeros();
    test_buf_roundtrip_all_ff();
    test_buf_single_byte();
    test_buf_with_correction();
    test_buf_odd_code_len();
    test_buf_output_too_small();
    test_codeword_7bit_range();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
