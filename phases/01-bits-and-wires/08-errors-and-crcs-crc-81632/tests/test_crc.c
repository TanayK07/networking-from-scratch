/* Tests for CRC-8 / CRC-16 / CRC-32 implementations.
 *
 * Test families:
 *   1.  Function existence / API smoke test
 *   2.  CRC-32 standard test vector ("123456789" == 0xCBF43926)
 *   3.  CRC-16/CCITT standard test vector ("123456789" == 0x29B1)
 *   4.  CRC-8 standard test vector ("123456789" == 0xF4)
 *   5.  Empty input
 *   6.  Single byte
 *   7.  Known Ethernet FCS value
 *   8.  CRC-32 residue check (append CRC to data, re-CRC == constant)
 *   9.  Property: CRC changes when any bit flips
 *   10. Multi-block consistency
 *
 * Compile + run:
 *     make
 *     ./test_crc
 */

#include "../crc.h"

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

/* The canonical test string for CRC verification. */
static const uint8_t test_data[] = "123456789";
static const size_t test_data_len = 9; /* strlen("123456789") */

/* ---------------------------------------------------------------
 * Test 1: API smoke test — functions exist and return values
 * --------------------------------------------------------------- */

static void test_api_smoke(void) {
    printf("  API smoke test...");

    /* Just call each function to prove they link and don't crash. */
    nfs_crc8_init_table();
    nfs_crc16_init_table();
    nfs_crc32_init_table();

    uint8_t data[] = {0x00};
    uint8_t c8 = nfs_crc8(data, 1);
    uint16_t c16 = nfs_crc16(data, 1);
    uint32_t c32 = nfs_crc32(data, 1);

    /* Values should be deterministic — same input always yields same output. */
    ASSERT_EQ(nfs_crc8(data, 1), c8);
    ASSERT_EQ(nfs_crc16(data, 1), c16);
    ASSERT_EQ(nfs_crc32(data, 1), c32);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 2: CRC-32 standard test vector
 *
 * The string "123456789" (without NUL) must produce 0xCBF43926.
 * This is the universal CRC-32 check value documented by every
 * CRC reference and matched by zlib's crc32().
 * --------------------------------------------------------------- */

static void test_crc32_check_value(void) {
    printf("  CRC-32 check value...");
    ASSERT_EQ(nfs_crc32(test_data, test_data_len), 0xCBF43926);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: CRC-16/CCITT standard test vector
 *
 * "123456789" with CRC-16/CCITT (poly 0x1021, init 0xFFFF) = 0x29B1.
 * --------------------------------------------------------------- */

static void test_crc16_check_value(void) {
    printf("  CRC-16 check value...");
    ASSERT_EQ(nfs_crc16(test_data, test_data_len), 0x29B1);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: CRC-8 standard test vector
 *
 * "123456789" with CRC-8 (poly 0x07, init 0x00) = 0xF4.
 * --------------------------------------------------------------- */

static void test_crc8_check_value(void) {
    printf("  CRC-8 check value...");
    ASSERT_EQ(nfs_crc8(test_data, test_data_len), 0xF4);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: Empty input
 *
 * CRC of zero-length data should equal the init value (possibly
 * XORed with the final mask).
 *   CRC-8:  init 0x00, no final XOR  → 0x00
 *   CRC-16: init 0xFFFF, no final XOR → 0xFFFF
 *   CRC-32: init 0xFFFFFFFF ^ 0xFFFFFFFF → 0x00000000
 * --------------------------------------------------------------- */

static void test_empty_input(void) {
    printf("  empty input...");
    ASSERT_EQ(nfs_crc8(NULL, 0), 0x00);
    ASSERT_EQ(nfs_crc16(NULL, 0), 0xFFFF);
    ASSERT_EQ(nfs_crc32(NULL, 0), 0x00000000);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: Single byte
 *
 * Verify CRC over a single byte {0x00} and {0xFF}.
 * --------------------------------------------------------------- */

static void test_single_byte(void) {
    printf("  single byte...");

    uint8_t zero = 0x00;
    uint8_t ff = 0xFF;

    /* CRC-8 of 0x00: table[0x00 ^ 0x00] = table[0] = 0x00 */
    ASSERT_EQ(nfs_crc8(&zero, 1), 0x00);
    /* CRC-8 of 0xFF: table[0x00 ^ 0xFF] = table[0xFF] */
    uint8_t c8_ff = nfs_crc8(&ff, 1);
    ASSERT_TRUE(c8_ff != 0x00); /* 0xFF should produce a non-zero CRC-8 */

    /* CRC-32 of single bytes should be deterministic. */
    uint32_t c32_zero = nfs_crc32(&zero, 1);
    uint32_t c32_ff = nfs_crc32(&ff, 1);
    ASSERT_TRUE(c32_zero != c32_ff); /* different inputs, different CRCs */

    /* Known CRC-32 values for single bytes (verified against zlib). */
    ASSERT_EQ(c32_zero, 0xD202EF8D);
    ASSERT_EQ(c32_ff, 0xFF000000);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: Known Ethernet FCS
 *
 * An Ethernet frame payload "Hello" (48 65 6c 6c 6f) should
 * produce a known CRC-32 that matches the Ethernet FCS.
 * --------------------------------------------------------------- */

static void test_ethernet_fcs(void) {
    printf("  Ethernet FCS...");

    /* "Hello" in ASCII */
    const uint8_t hello[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f};
    uint32_t crc = nfs_crc32(hello, sizeof(hello));

    /* This is the standard CRC-32 of "Hello", which matches
     * what Ethernet would use as the FCS. */
    ASSERT_EQ(crc, 0xF7D18982);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: CRC-32 residue check
 *
 * Property: if you append the CRC-32 (in little-endian byte order)
 * to the original data and compute CRC-32 over the combined buffer,
 * you always get the constant residue 0x2144DF1C.
 *
 * This is how Ethernet FCS verification works: the receiver
 * runs CRC-32 over the entire frame (data + FCS) and checks
 * for this magic residue.
 * --------------------------------------------------------------- */

static void test_crc32_residue(void) {
    printf("  CRC-32 residue...");

    const uint8_t data[] = "Hello, CRC!";
    size_t dlen = 11; /* strlen("Hello, CRC!") */

    uint32_t crc = nfs_crc32(data, dlen);

    /* Append CRC in little-endian order (as Ethernet does). */
    uint8_t combined[15]; /* 11 + 4 */
    memcpy(combined, data, dlen);
    combined[dlen + 0] = (uint8_t)(crc & 0xFF);
    combined[dlen + 1] = (uint8_t)((crc >> 8) & 0xFF);
    combined[dlen + 2] = (uint8_t)((crc >> 16) & 0xFF);
    combined[dlen + 3] = (uint8_t)((crc >> 24) & 0xFF);

    uint32_t residue = nfs_crc32(combined, dlen + 4);
    ASSERT_EQ(residue, 0x2144DF1C);

    /* Verify with a second message to confirm it's universal. */
    const uint8_t data2[] = "Test";
    size_t dlen2 = 4;
    uint32_t crc2 = nfs_crc32(data2, dlen2);
    uint8_t combined2[8]; /* 4 + 4 */
    memcpy(combined2, data2, dlen2);
    combined2[dlen2 + 0] = (uint8_t)(crc2 & 0xFF);
    combined2[dlen2 + 1] = (uint8_t)((crc2 >> 8) & 0xFF);
    combined2[dlen2 + 2] = (uint8_t)((crc2 >> 16) & 0xFF);
    combined2[dlen2 + 3] = (uint8_t)((crc2 >> 24) & 0xFF);

    uint32_t residue2 = nfs_crc32(combined2, dlen2 + 4);
    ASSERT_EQ(residue2, 0x2144DF1C);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: Property — CRC changes when any single bit flips
 *
 * This is the fundamental guarantee of CRC: any single-bit error
 * in the data produces a different CRC value.
 * --------------------------------------------------------------- */

static void test_bit_flip_detection(void) {
    printf("  bit flip detection...");

    const uint8_t original[] = "NetworkingFromScratch";
    size_t len = 21; /* strlen */

    uint32_t original_crc32 = nfs_crc32(original, len);
    uint16_t original_crc16 = nfs_crc16(original, len);
    uint8_t original_crc8 = nfs_crc8(original, len);

    /* Flip every single bit in the data and verify CRC changes. */
    uint8_t modified[21];
    for (size_t byte_idx = 0; byte_idx < len; byte_idx++) {
        for (int bit = 0; bit < 8; bit++) {
            memcpy(modified, original, len);
            modified[byte_idx] ^= (uint8_t)(1 << bit);

            ASSERT_TRUE(nfs_crc32(modified, len) != original_crc32);
            ASSERT_TRUE(nfs_crc16(modified, len) != original_crc16);
            ASSERT_TRUE(nfs_crc8(modified, len) != original_crc8);
        }
    }

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 10: Multi-block consistency
 *
 * CRC of a concatenated buffer must be deterministic and differ
 * from the CRC of either sub-buffer alone.
 * --------------------------------------------------------------- */

static void test_multiblock_consistency(void) {
    printf("  multi-block consistency...");

    const uint8_t part_a[] = "Hello";
    const uint8_t part_b[] = "World";
    uint8_t combined[10];
    memcpy(combined, part_a, 5);
    memcpy(combined + 5, part_b, 5);

    uint32_t crc_a = nfs_crc32(part_a, 5);
    uint32_t crc_b = nfs_crc32(part_b, 5);
    uint32_t crc_ab = nfs_crc32(combined, 10);

    /* The CRC of the concatenation should differ from either part. */
    ASSERT_TRUE(crc_ab != crc_a);
    ASSERT_TRUE(crc_ab != crc_b);

    /* But it must be deterministic. */
    ASSERT_EQ(nfs_crc32(combined, 10), crc_ab);

    /* Same for CRC-16. */
    uint16_t crc16_a = nfs_crc16(part_a, 5);
    uint16_t crc16_b = nfs_crc16(part_b, 5);
    uint16_t crc16_ab = nfs_crc16(combined, 10);
    ASSERT_TRUE(crc16_ab != crc16_a);
    ASSERT_TRUE(crc16_ab != crc16_b);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void) {
    printf("CRC-8/16/32 test suite\n");
    test_api_smoke();
    test_crc32_check_value();
    test_crc16_check_value();
    test_crc8_check_value();
    test_empty_input();
    test_single_byte();
    test_ethernet_fcs();
    test_crc32_residue();
    test_bit_flip_detection();
    test_multiblock_consistency();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
