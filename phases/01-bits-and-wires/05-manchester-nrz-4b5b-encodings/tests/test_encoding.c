/* Unit tests for Manchester, NRZI, and 4B/5B encodings. */

#include "../encoding.h"
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

/* ---- Manchester tests ---- */

static void test_manchester_single_byte_roundtrip(void) {
    uint8_t data[] = {0xA5};
    uint8_t symbols[32];
    uint8_t decoded[1];

    size_t nsym = nfs_manchester_encode(data, 1, symbols, sizeof(symbols));
    ASSERT_EQ(nsym, 16);

    int n = nfs_manchester_decode(symbols, nsym, decoded, sizeof(decoded));
    ASSERT_EQ(n, 1);
    ASSERT_EQ(decoded[0], 0xA5);
}

static void test_manchester_all_zeros(void) {
    uint8_t data[] = {0x00};
    uint8_t symbols[16];
    uint8_t decoded[1];

    size_t nsym = nfs_manchester_encode(data, 1, symbols, sizeof(symbols));
    ASSERT_EQ(nsym, 16);

    /* 0 → high-low → (1,0) for each bit */
    for (int i = 0; i < 16; i += 2) {
        ASSERT_EQ(symbols[i], 1);
        ASSERT_EQ(symbols[i + 1], 0);
    }

    int n = nfs_manchester_decode(symbols, nsym, decoded, sizeof(decoded));
    ASSERT_EQ(n, 1);
    ASSERT_EQ(decoded[0], 0x00);
}

static void test_manchester_all_ones(void) {
    uint8_t data[] = {0xFF};
    uint8_t symbols[16];
    uint8_t decoded[1];

    size_t nsym = nfs_manchester_encode(data, 1, symbols, sizeof(symbols));
    ASSERT_EQ(nsym, 16);

    /* 1 → low-high → (0,1) for each bit */
    for (int i = 0; i < 16; i += 2) {
        ASSERT_EQ(symbols[i], 0);
        ASSERT_EQ(symbols[i + 1], 1);
    }

    int n = nfs_manchester_decode(symbols, nsym, decoded, sizeof(decoded));
    ASSERT_EQ(n, 1);
    ASSERT_EQ(decoded[0], 0xFF);
}

static void test_manchester_multi_byte(void) {
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t symbols[64];
    uint8_t decoded[4];

    size_t nsym = nfs_manchester_encode(data, 4, symbols, sizeof(symbols));
    ASSERT_EQ(nsym, 64);

    int n = nfs_manchester_decode(symbols, nsym, decoded, sizeof(decoded));
    ASSERT_EQ(n, 4);
    ASSERT_EQ(decoded[0], 0xDE);
    ASSERT_EQ(decoded[1], 0xAD);
    ASSERT_EQ(decoded[2], 0xBE);
    ASSERT_EQ(decoded[3], 0xEF);
}

static void test_manchester_invalid_transition(void) {
    /* Two identical symbols (0,0) is invalid Manchester */
    uint8_t bad_symbols[16] = {0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0};
    uint8_t decoded[1];
    int n = nfs_manchester_decode(bad_symbols, 16, decoded, sizeof(decoded));
    ASSERT_EQ(n, -1);
}

static void test_manchester_invalid_length(void) {
    /* Odd number of symbols */
    uint8_t symbols[15] = {0};
    uint8_t decoded[1];
    int n = nfs_manchester_decode(symbols, 15, decoded, sizeof(decoded));
    ASSERT_EQ(n, -1);
}

static void test_manchester_known_pattern(void) {
    /* 0x80 = 10000000 → Manchester:
     * bit 1: (0,1)
     * bits 0..0: (1,0) each
     * symbols: 0 1 1 0 1 0 1 0 1 0 1 0 1 0 1 0
     */
    uint8_t data[] = {0x80};
    uint8_t symbols[16];

    size_t nsym = nfs_manchester_encode(data, 1, symbols, sizeof(symbols));
    ASSERT_EQ(nsym, 16);
    ASSERT_EQ(symbols[0], 0);
    ASSERT_EQ(symbols[1], 1);
    ASSERT_EQ(symbols[2], 1);
    ASSERT_EQ(symbols[3], 0);
}

/* ---- NRZI tests ---- */

static void test_nrzi_single_byte_roundtrip(void) {
    uint8_t data[] = {0xA5};
    uint8_t symbols[8];
    uint8_t decoded[1];

    size_t nsym = nfs_nrzi_encode(data, 1, symbols, sizeof(symbols));
    ASSERT_EQ(nsym, 8);

    int n = nfs_nrzi_decode(symbols, nsym, decoded, sizeof(decoded));
    ASSERT_EQ(n, 1);
    ASSERT_EQ(decoded[0], 0xA5);
}

static void test_nrzi_all_zeros(void) {
    /* All zeros → no transitions → all symbols stay at initial level (0) */
    uint8_t data[] = {0x00};
    uint8_t symbols[8];

    size_t nsym = nfs_nrzi_encode(data, 1, symbols, sizeof(symbols));
    ASSERT_EQ(nsym, 8);
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(symbols[i], 0);
    }
}

static void test_nrzi_all_ones(void) {
    /* All ones → toggle every bit.
     * Starting at 0: 1, 0, 1, 0, 1, 0, 1, 0 */
    uint8_t data[] = {0xFF};
    uint8_t symbols[8];

    size_t nsym = nfs_nrzi_encode(data, 1, symbols, sizeof(symbols));
    ASSERT_EQ(nsym, 8);
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(symbols[i], (uint8_t)(i % 2 == 0 ? 1 : 0));
    }
}

static void test_nrzi_multi_byte_roundtrip(void) {
    uint8_t data[] = {0x12, 0x34, 0x56, 0x78, 0x9A};
    uint8_t symbols[40];
    uint8_t decoded[5];

    size_t nsym = nfs_nrzi_encode(data, 5, symbols, sizeof(symbols));
    ASSERT_EQ(nsym, 40);

    int n = nfs_nrzi_decode(symbols, nsym, decoded, sizeof(decoded));
    ASSERT_EQ(n, 5);
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(decoded[i], data[i]);
    }
}

static void test_nrzi_invalid_length(void) {
    uint8_t symbols[7] = {0};
    uint8_t decoded[1];
    int n = nfs_nrzi_decode(symbols, 7, decoded, sizeof(decoded));
    ASSERT_EQ(n, -1);
}

static void test_nrzi_known_pattern(void) {
    /* 0x80 = 10000000
     * Initial level=0
     * bit 1: toggle → level=1
     * bits 0: no change → 1,1,1,1,1,1,1
     * symbols: 1 1 1 1 1 1 1 1  */
    uint8_t data[] = {0x80};
    uint8_t symbols[8];

    size_t nsym = nfs_nrzi_encode(data, 1, symbols, sizeof(symbols));
    ASSERT_EQ(nsym, 8);
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(symbols[i], 1);
    }
}

/* ---- 4B/5B tests ---- */

static void test_4b5b_table_correctness(void) {
    /* Verify the exact codes from the standard table */
    ASSERT_EQ(nfs_4b5b_encode(0x0), 0x1E); /* 11110 */
    ASSERT_EQ(nfs_4b5b_encode(0x1), 0x09); /* 01001 */
    ASSERT_EQ(nfs_4b5b_encode(0x2), 0x14); /* 10100 */
    ASSERT_EQ(nfs_4b5b_encode(0x3), 0x15); /* 10101 */
    ASSERT_EQ(nfs_4b5b_encode(0x4), 0x0A); /* 01010 */
    ASSERT_EQ(nfs_4b5b_encode(0x5), 0x0B); /* 01011 */
    ASSERT_EQ(nfs_4b5b_encode(0x6), 0x0E); /* 01110 */
    ASSERT_EQ(nfs_4b5b_encode(0x7), 0x0F); /* 01111 */
    ASSERT_EQ(nfs_4b5b_encode(0x8), 0x12); /* 10010 */
    ASSERT_EQ(nfs_4b5b_encode(0x9), 0x13); /* 10011 */
    ASSERT_EQ(nfs_4b5b_encode(0xA), 0x16); /* 10110 */
    ASSERT_EQ(nfs_4b5b_encode(0xB), 0x17); /* 10111 */
    ASSERT_EQ(nfs_4b5b_encode(0xC), 0x1A); /* 11010 */
    ASSERT_EQ(nfs_4b5b_encode(0xD), 0x1B); /* 11011 */
    ASSERT_EQ(nfs_4b5b_encode(0xE), 0x1C); /* 11100 */
    ASSERT_EQ(nfs_4b5b_encode(0xF), 0x1D); /* 11101 */
}

static void test_4b5b_roundtrip_all(void) {
    /* Every nibble must roundtrip through encode→decode */
    for (uint8_t i = 0; i < 16; i++) {
        int code = nfs_4b5b_encode(i);
        ASSERT_TRUE(code >= 0);
        int dec = nfs_4b5b_decode((uint8_t)code);
        ASSERT_EQ(dec, i);
    }
}

static void test_4b5b_invalid_nibble(void) {
    ASSERT_EQ(nfs_4b5b_encode(0x10), -1);
    ASSERT_EQ(nfs_4b5b_encode(0xFF), -1);
}

static void test_4b5b_invalid_codes(void) {
    /* Codes not in the data table should return -1 */
    ASSERT_EQ(nfs_4b5b_decode(0x00), -1); /* 00000 - not used */
    ASSERT_EQ(nfs_4b5b_decode(0x01), -1); /* 00001 */
    ASSERT_EQ(nfs_4b5b_decode(0x02), -1); /* 00010 */
    ASSERT_EQ(nfs_4b5b_decode(0x1F), -1); /* 11111 */
}

static void test_4b5b_no_more_than_3_zeros(void) {
    /* Verify that no valid 5-bit code has more than 1 leading zero
     * and no more than 2 consecutive zeros total.
     * Actually the guarantee is: no run of 3+ zeros in the code.
     * This is the whole point of 4B/5B. */
    for (uint8_t i = 0; i < 16; i++) {
        int code = nfs_4b5b_encode(i);
        ASSERT_TRUE(code >= 0);
        /* Check for 3+ consecutive zeros */
        int consecutive = 0;
        for (int bit = 4; bit >= 0; bit--) {
            if (((code >> bit) & 1) == 0) {
                consecutive++;
                ASSERT_TRUE(consecutive < 3);
            } else {
                consecutive = 0;
            }
        }
    }
}

static void test_manchester_buffer_too_small(void) {
    uint8_t data[] = {0xFF};
    uint8_t symbols[8]; /* too small, need 16 */
    size_t nsym = nfs_manchester_encode(data, 1, symbols, sizeof(symbols));
    ASSERT_EQ(nsym, 0);
}

static void test_nrzi_buffer_too_small(void) {
    uint8_t data[] = {0xFF};
    uint8_t symbols[4]; /* too small, need 8 */
    size_t nsym = nfs_nrzi_encode(data, 1, symbols, sizeof(symbols));
    ASSERT_EQ(nsym, 0);
}

static void test_manchester_all_byte_values(void) {
    /* Roundtrip all 256 byte values */
    for (int v = 0; v < 256; v++) {
        uint8_t data = (uint8_t)v;
        uint8_t symbols[16];
        uint8_t decoded;

        size_t nsym = nfs_manchester_encode(&data, 1, symbols, sizeof(symbols));
        ASSERT_EQ(nsym, 16);

        int n = nfs_manchester_decode(symbols, nsym, &decoded, 1);
        ASSERT_EQ(n, 1);
        ASSERT_EQ(decoded, data);
    }
}

static void test_nrzi_all_byte_values(void) {
    /* Roundtrip all 256 byte values */
    for (int v = 0; v < 256; v++) {
        uint8_t data = (uint8_t)v;
        uint8_t symbols[8];
        uint8_t decoded;

        size_t nsym = nfs_nrzi_encode(&data, 1, symbols, sizeof(symbols));
        ASSERT_EQ(nsym, 8);

        int n = nfs_nrzi_decode(symbols, nsym, &decoded, 1);
        ASSERT_EQ(n, 1);
        ASSERT_EQ(decoded, data);
    }
}

int main(void) {
    printf("Encoding Tests\n");

    test_manchester_single_byte_roundtrip();
    test_manchester_all_zeros();
    test_manchester_all_ones();
    test_manchester_multi_byte();
    test_manchester_invalid_transition();
    test_manchester_invalid_length();
    test_manchester_known_pattern();
    test_nrzi_single_byte_roundtrip();
    test_nrzi_all_zeros();
    test_nrzi_all_ones();
    test_nrzi_multi_byte_roundtrip();
    test_nrzi_invalid_length();
    test_nrzi_known_pattern();
    test_4b5b_table_correctness();
    test_4b5b_roundtrip_all();
    test_4b5b_invalid_nibble();
    test_4b5b_invalid_codes();
    test_4b5b_no_more_than_3_zeros();
    test_manchester_buffer_too_small();
    test_nrzi_buffer_too_small();
    test_manchester_all_byte_values();
    test_nrzi_all_byte_values();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
