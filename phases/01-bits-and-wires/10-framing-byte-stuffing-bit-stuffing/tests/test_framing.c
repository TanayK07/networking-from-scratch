/* Unit tests for byte stuffing, bit stuffing, and COBS. */

#include "../framing.h"
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

/* ---- Byte stuffing tests ---- */

static void test_byte_stuff_empty(void) {
    uint8_t out[16];
    int n = nfs_byte_stuff((const uint8_t *)"", 0, out, sizeof(out));
    /* Empty payload: just two flags */
    ASSERT_EQ(n, 2);
    ASSERT_EQ(out[0], 0x7E);
    ASSERT_EQ(out[1], 0x7E);
}

static void test_byte_stuff_no_escape(void) {
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint8_t out[16];
    int n = nfs_byte_stuff(data, 3, out, sizeof(out));
    ASSERT_EQ(n, 5); /* flag + 3 data + flag */
    ASSERT_EQ(out[0], 0x7E);
    ASSERT_EQ(out[1], 0x01);
    ASSERT_EQ(out[2], 0x02);
    ASSERT_EQ(out[3], 0x03);
    ASSERT_EQ(out[4], 0x7E);
}

static void test_byte_stuff_flag_escape(void) {
    uint8_t data[] = {0x7E};
    uint8_t out[16];
    int n = nfs_byte_stuff(data, 1, out, sizeof(out));
    ASSERT_EQ(n, 4); /* flag + escape + escaped_val + flag */
    ASSERT_EQ(out[0], 0x7E);
    ASSERT_EQ(out[1], 0x7D);
    ASSERT_EQ(out[2], 0x5E);
    ASSERT_EQ(out[3], 0x7E);
}

static void test_byte_stuff_escape_escape(void) {
    uint8_t data[] = {0x7D};
    uint8_t out[16];
    int n = nfs_byte_stuff(data, 1, out, sizeof(out));
    ASSERT_EQ(n, 4);
    ASSERT_EQ(out[1], 0x7D);
    ASSERT_EQ(out[2], 0x5D);
}

static void test_byte_stuff_mixed(void) {
    uint8_t data[] = {0x01, 0x7E, 0x02, 0x7D, 0x03};
    uint8_t out[32];
    int n = nfs_byte_stuff(data, 5, out, sizeof(out));
    /* 2 flags + 3 normal + 2*2 escaped = 2 + 3 + 4 = 9 */
    ASSERT_EQ(n, 9);
}

static void test_byte_unstuff_roundtrip(void) {
    uint8_t data[] = {0x01, 0x7E, 0x02, 0x7D, 0x03};
    uint8_t frame[32];
    uint8_t recovered[32];

    int flen = nfs_byte_stuff(data, 5, frame, sizeof(frame));
    ASSERT_TRUE(flen > 0);

    int rlen = nfs_byte_unstuff(frame, (size_t)flen, recovered, sizeof(recovered));
    ASSERT_EQ(rlen, 5);
    ASSERT_EQ(memcmp(data, recovered, 5), 0);
}

static void test_byte_unstuff_empty_frame(void) {
    uint8_t frame[] = {0x7E, 0x7E};
    uint8_t out[16];
    int n = nfs_byte_unstuff(frame, 2, out, sizeof(out));
    ASSERT_EQ(n, 0);
}

static void test_byte_unstuff_no_start_flag(void) {
    uint8_t frame[] = {0x01, 0x7E};
    uint8_t out[16];
    int n = nfs_byte_unstuff(frame, 2, out, sizeof(out));
    ASSERT_EQ(n, -1);
}

static void test_byte_unstuff_no_end_flag(void) {
    uint8_t frame[] = {0x7E, 0x01};
    uint8_t out[16];
    int n = nfs_byte_unstuff(frame, 2, out, sizeof(out));
    ASSERT_EQ(n, -1);
}

static void test_byte_stuff_all_flags(void) {
    uint8_t data[] = {0x7E, 0x7E, 0x7E};
    uint8_t frame[32];
    uint8_t recovered[32];

    int flen = nfs_byte_stuff(data, 3, frame, sizeof(frame));
    ASSERT_TRUE(flen > 0);

    int rlen = nfs_byte_unstuff(frame, (size_t)flen, recovered, sizeof(recovered));
    ASSERT_EQ(rlen, 3);
    ASSERT_EQ(recovered[0], 0x7E);
    ASSERT_EQ(recovered[1], 0x7E);
    ASSERT_EQ(recovered[2], 0x7E);
}

static void test_byte_stuff_buffer_too_small(void) {
    uint8_t data[] = {0x7E};
    uint8_t out[2]; /* too small: need at least 4 */
    int n = nfs_byte_stuff(data, 1, out, sizeof(out));
    ASSERT_EQ(n, -1);
}

/* ---- Bit stuffing tests ---- */

static void test_bit_stuff_no_stuffing_needed(void) {
    /* 0xA5 = 10100101 — no run of 5 ones */
    uint8_t data[] = {0xA5};
    uint8_t out[4];
    int nbits = nfs_bit_stuff(data, 8, out, sizeof(out));
    ASSERT_EQ(nbits, 8); /* No stuffing needed */
}

static void test_bit_stuff_five_ones(void) {
    /* 0xF8 = 11111000 — five 1s then three 0s
     * After stuffing: 111110000 (9 bits) — 0 inserted after five 1s */
    uint8_t data[] = {0xF8};
    uint8_t out[4];
    int nbits = nfs_bit_stuff(data, 8, out, sizeof(out));
    ASSERT_EQ(nbits, 9);
}

static void test_bit_stuff_all_ones(void) {
    /* 0xFF = 11111111
     * Data: 1 1 1 1 1 1 1 1
     * After bit 4 (5th consecutive 1): insert 0
     * Continue: bit 5=1, bit 6=1, bit 7=1 (only 3 consecutive)
     * Output: 1 1 1 1 1 0 1 1 1 = 9 bits */
    uint8_t data[] = {0xFF};
    uint8_t out[4];
    memset(out, 0, sizeof(out));
    int nbits = nfs_bit_stuff(data, 8, out, sizeof(out));
    ASSERT_EQ(nbits, 9);
}

static void test_bit_stuff_roundtrip_simple(void) {
    uint8_t data[] = {0xFF};
    uint8_t stuffed[4];
    uint8_t recovered[4];
    memset(stuffed, 0, sizeof(stuffed));
    memset(recovered, 0, sizeof(recovered));

    int sbits = nfs_bit_stuff(data, 8, stuffed, sizeof(stuffed));
    ASSERT_TRUE(sbits > 0);

    int rbits = nfs_bit_unstuff(stuffed, (size_t)sbits, recovered, sizeof(recovered));
    ASSERT_EQ(rbits, 8);
    ASSERT_EQ(recovered[0], 0xFF);
}

static void test_bit_stuff_roundtrip_multi(void) {
    uint8_t data[] = {0xFF, 0xFF, 0x00, 0xFF};
    uint8_t stuffed[16];
    uint8_t recovered[16];
    memset(stuffed, 0, sizeof(stuffed));
    memset(recovered, 0, sizeof(recovered));

    int sbits = nfs_bit_stuff(data, 32, stuffed, sizeof(stuffed));
    ASSERT_TRUE(sbits > 32); /* Stuffing adds bits */

    int rbits = nfs_bit_unstuff(stuffed, (size_t)sbits, recovered, sizeof(recovered));
    ASSERT_EQ(rbits, 32);
    ASSERT_EQ(recovered[0], 0xFF);
    ASSERT_EQ(recovered[1], 0xFF);
    ASSERT_EQ(recovered[2], 0x00);
    ASSERT_EQ(recovered[3], 0xFF);
}

static void test_bit_stuff_all_zeros(void) {
    uint8_t data[] = {0x00};
    uint8_t out[4];
    int nbits = nfs_bit_stuff(data, 8, out, sizeof(out));
    ASSERT_EQ(nbits, 8); /* No stuffing for zeros */
}

static void test_bit_stuff_six_ones(void) {
    /* 0xFC = 11111100 — six 1s then two 0s
     * After bit 4 (5th 1): stuff 0
     * Bit 5 (6th 1): emit 1
     * Bits 6-7 (0s): emit 00
     * Output: 11111 0 1 00 = 9 bits */
    uint8_t data[] = {0xFC};
    uint8_t stuffed[4];
    uint8_t recovered[4];
    memset(stuffed, 0, sizeof(stuffed));
    memset(recovered, 0, sizeof(recovered));

    int sbits = nfs_bit_stuff(data, 8, stuffed, sizeof(stuffed));
    ASSERT_EQ(sbits, 9);

    int rbits = nfs_bit_unstuff(stuffed, (size_t)sbits, recovered, sizeof(recovered));
    ASSERT_EQ(rbits, 8);
    ASSERT_EQ(recovered[0], 0xFC);
}

static void test_bit_stuff_ten_ones(void) {
    /* 10 consecutive 1-bits: 0xFF + 0xC0 (top 2 bits)
     * First 5: stuff, next 5: stuff
     * Total: 10 data + 2 stuff = 12 bits */
    uint8_t data[] = {0xFF, 0xC0};
    uint8_t stuffed[4];
    uint8_t recovered[4];
    memset(stuffed, 0, sizeof(stuffed));
    memset(recovered, 0, sizeof(recovered));

    int sbits = nfs_bit_stuff(data, 10, stuffed, sizeof(stuffed));
    ASSERT_EQ(sbits, 12); /* 10 data + 2 stuffed zeros */

    int rbits = nfs_bit_unstuff(stuffed, (size_t)sbits, recovered, sizeof(recovered));
    ASSERT_EQ(rbits, 10);
}

/* ---- COBS tests ---- */

static void test_cobs_empty(void) {
    uint8_t out[4];
    int n = nfs_cobs_encode((const uint8_t *)"", 0, out, sizeof(out));
    ASSERT_EQ(n, 1); /* Just the overhead byte */
    ASSERT_EQ(out[0], 0x01);
}

static void test_cobs_single_zero(void) {
    uint8_t data[] = {0x00};
    uint8_t encoded[4];
    int n = nfs_cobs_encode(data, 1, encoded, sizeof(encoded));
    ASSERT_EQ(n, 2);
    ASSERT_EQ(encoded[0], 0x01);
    ASSERT_EQ(encoded[1], 0x01);
}

static void test_cobs_single_nonzero(void) {
    uint8_t data[] = {0x42};
    uint8_t encoded[4];
    int n = nfs_cobs_encode(data, 1, encoded, sizeof(encoded));
    ASSERT_EQ(n, 2);
    ASSERT_EQ(encoded[0], 0x02);
    ASSERT_EQ(encoded[1], 0x42);
}

static void test_cobs_known_example(void) {
    /* Standard COBS example:
     * Input:  00 11 00 00 22
     * Encoded: 01 02 11 01 02 22  */
    uint8_t data[] = {0x00, 0x11, 0x00, 0x00, 0x22};
    uint8_t encoded[16];
    int n = nfs_cobs_encode(data, 5, encoded, sizeof(encoded));
    ASSERT_EQ(n, 6);
    ASSERT_EQ(encoded[0], 0x01);
    ASSERT_EQ(encoded[1], 0x02);
    ASSERT_EQ(encoded[2], 0x11);
    ASSERT_EQ(encoded[3], 0x01);
    ASSERT_EQ(encoded[4], 0x02);
    ASSERT_EQ(encoded[5], 0x22);
}

static void test_cobs_no_zeros(void) {
    uint8_t data[] = {0x11, 0x22, 0x33};
    uint8_t encoded[8];
    int n = nfs_cobs_encode(data, 3, encoded, sizeof(encoded));
    ASSERT_EQ(n, 4);
    ASSERT_EQ(encoded[0], 0x04);
    ASSERT_EQ(encoded[1], 0x11);
    ASSERT_EQ(encoded[2], 0x22);
    ASSERT_EQ(encoded[3], 0x33);
}

static void test_cobs_all_zeros(void) {
    uint8_t data[] = {0x00, 0x00, 0x00};
    uint8_t encoded[8];
    int n = nfs_cobs_encode(data, 3, encoded, sizeof(encoded));
    ASSERT_EQ(n, 4);
    ASSERT_EQ(encoded[0], 0x01);
    ASSERT_EQ(encoded[1], 0x01);
    ASSERT_EQ(encoded[2], 0x01);
    ASSERT_EQ(encoded[3], 0x01);
}

static void test_cobs_roundtrip_simple(void) {
    uint8_t data[] = {0x00, 0x11, 0x00, 0x00, 0x22};
    uint8_t encoded[16];
    uint8_t decoded[16];

    int elen = nfs_cobs_encode(data, 5, encoded, sizeof(encoded));
    ASSERT_TRUE(elen > 0);

    /* Verify no zeros in encoded data */
    for (int i = 0; i < elen; i++) {
        ASSERT_TRUE(encoded[i] != 0x00);
    }

    int dlen = nfs_cobs_decode(encoded, (size_t)elen, decoded, sizeof(decoded));
    ASSERT_EQ(dlen, 5);
    ASSERT_EQ(memcmp(data, decoded, 5), 0);
}

static void test_cobs_roundtrip_no_zeros(void) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t encoded[16];
    uint8_t decoded[16];

    int elen = nfs_cobs_encode(data, 5, encoded, sizeof(encoded));
    ASSERT_TRUE(elen > 0);

    int dlen = nfs_cobs_decode(encoded, (size_t)elen, decoded, sizeof(decoded));
    ASSERT_EQ(dlen, 5);
    ASSERT_EQ(memcmp(data, decoded, 5), 0);
}

static void test_cobs_roundtrip_all_zeros(void) {
    uint8_t data[8];
    memset(data, 0x00, 8);
    uint8_t encoded[16];
    uint8_t decoded[16];

    int elen = nfs_cobs_encode(data, 8, encoded, sizeof(encoded));
    ASSERT_TRUE(elen > 0);

    int dlen = nfs_cobs_decode(encoded, (size_t)elen, decoded, sizeof(decoded));
    ASSERT_EQ(dlen, 8);
    ASSERT_EQ(memcmp(data, decoded, 8), 0);
}

static void test_cobs_decode_invalid_zero(void) {
    /* A zero byte in COBS-encoded data is invalid */
    uint8_t bad[] = {0x02, 0x00};
    uint8_t out[4];
    int n = nfs_cobs_decode(bad, 2, out, sizeof(out));
    ASSERT_EQ(n, -1);
}

static void test_cobs_roundtrip_longer(void) {
    uint8_t data[32];
    for (int i = 0; i < 32; i++)
        data[i] = (uint8_t)(i * 8); /* includes zeros at i=0 and i=32 */

    uint8_t encoded[64];
    uint8_t decoded[64];

    int elen = nfs_cobs_encode(data, 32, encoded, sizeof(encoded));
    ASSERT_TRUE(elen > 0);

    int dlen = nfs_cobs_decode(encoded, (size_t)elen, decoded, sizeof(decoded));
    ASSERT_EQ(dlen, 32);
    ASSERT_EQ(memcmp(data, decoded, 32), 0);
}

static void test_byte_stuff_roundtrip_many_patterns(void) {
    /* Test roundtrip with various byte patterns */
    uint8_t patterns[][4] = {
        {0x00, 0x00, 0x00, 0x00},
        {0xFF, 0xFF, 0xFF, 0xFF},
        {0x7E, 0x7D, 0x7E, 0x7D},
        {0x7D, 0x7E, 0x7D, 0x7E},
    };
    for (int p = 0; p < 4; p++) {
        uint8_t frame[32];
        uint8_t recovered[32];

        int flen = nfs_byte_stuff(patterns[p], 4, frame, sizeof(frame));
        ASSERT_TRUE(flen > 0);

        int rlen = nfs_byte_unstuff(frame, (size_t)flen, recovered, sizeof(recovered));
        ASSERT_EQ(rlen, 4);
        ASSERT_EQ(memcmp(patterns[p], recovered, 4), 0);
    }
}

int main(void) {
    printf("Framing Tests\n");

    test_byte_stuff_empty();
    test_byte_stuff_no_escape();
    test_byte_stuff_flag_escape();
    test_byte_stuff_escape_escape();
    test_byte_stuff_mixed();
    test_byte_unstuff_roundtrip();
    test_byte_unstuff_empty_frame();
    test_byte_unstuff_no_start_flag();
    test_byte_unstuff_no_end_flag();
    test_byte_stuff_all_flags();
    test_byte_stuff_buffer_too_small();
    test_bit_stuff_no_stuffing_needed();
    test_bit_stuff_five_ones();
    test_bit_stuff_all_ones();
    test_bit_stuff_roundtrip_simple();
    test_bit_stuff_roundtrip_multi();
    test_bit_stuff_all_zeros();
    test_bit_stuff_six_ones();
    test_bit_stuff_ten_ones();
    test_cobs_empty();
    test_cobs_single_zero();
    test_cobs_single_nonzero();
    test_cobs_known_example();
    test_cobs_no_zeros();
    test_cobs_all_zeros();
    test_cobs_roundtrip_simple();
    test_cobs_roundtrip_no_zeros();
    test_cobs_roundtrip_all_zeros();
    test_cobs_decode_invalid_zero();
    test_cobs_roundtrip_longer();
    test_byte_stuff_roundtrip_many_patterns();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
