/* Unit tests for nfs_hexdump functions. */

#include "../hexdump.h"
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

/* ---- hex_to_bytes tests ---- */

static void test_hex_to_bytes_simple(void) {
    uint8_t out[16];
    int n = nfs_hex_to_bytes("4500003c", out, sizeof(out));
    ASSERT_EQ(n, 4);
    ASSERT_EQ(out[0], 0x45);
    ASSERT_EQ(out[1], 0x00);
    ASSERT_EQ(out[2], 0x00);
    ASSERT_EQ(out[3], 0x3c);
}

static void test_hex_to_bytes_with_spaces(void) {
    uint8_t out[16];
    int n = nfs_hex_to_bytes("45 00 00 3c", out, sizeof(out));
    ASSERT_EQ(n, 4);
    ASSERT_EQ(out[0], 0x45);
    ASSERT_EQ(out[1], 0x00);
    ASSERT_EQ(out[2], 0x00);
    ASSERT_EQ(out[3], 0x3c);
}

static void test_hex_to_bytes_uppercase(void) {
    uint8_t out[4];
    int n = nfs_hex_to_bytes("DEADBEEF", out, sizeof(out));
    ASSERT_EQ(n, 4);
    ASSERT_EQ(out[0], 0xDE);
    ASSERT_EQ(out[1], 0xAD);
    ASSERT_EQ(out[2], 0xBE);
    ASSERT_EQ(out[3], 0xEF);
}

static void test_hex_to_bytes_empty(void) {
    uint8_t out[4];
    int n = nfs_hex_to_bytes("", out, sizeof(out));
    ASSERT_EQ(n, 0);
}

static void test_hex_to_bytes_invalid(void) {
    uint8_t out[4];
    int n = nfs_hex_to_bytes("ZZ", out, sizeof(out));
    ASSERT_EQ(n, -1);
}

static void test_hex_to_bytes_odd_length(void) {
    uint8_t out[4];
    int n = nfs_hex_to_bytes("ABC", out, sizeof(out));
    ASSERT_EQ(n, -1);
}

static void test_hex_to_bytes_buffer_too_small(void) {
    uint8_t out[1];
    int n = nfs_hex_to_bytes("AABBCCDD", out, sizeof(out));
    ASSERT_EQ(n, -1);
}

/* ---- hexdump_line tests ---- */

static void test_hexdump_line_single_byte(void) {
    uint8_t data[] = {0x41}; /* 'A' */
    char out[128];
    int n = nfs_hexdump_line(data, 1, 0, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    /* Should start with offset 00000000 */
    ASSERT_TRUE(strncmp(out, "00000000  41", 12) == 0);
    /* Should contain ASCII pane with 'A' */
    ASSERT_TRUE(strstr(out, "|A|") != NULL);
}

static void test_hexdump_line_16_bytes(void) {
    uint8_t data[16];
    for (int i = 0; i < 16; i++)
        data[i] = (uint8_t)i;
    char out[128];
    int n = nfs_hexdump_line(data, 16, 0, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    /* Check offset */
    ASSERT_TRUE(strncmp(out, "00000000  00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e 0f", 58) ==
                0);
}

static void test_hexdump_line_non_printable(void) {
    uint8_t data[] = {0x00, 0x01, 0x1F, 0x7F};
    char out[128];
    int n = nfs_hexdump_line(data, 4, 0, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    /* Non-printable chars should be '.' in ASCII pane */
    ASSERT_TRUE(strstr(out, "|....|") != NULL);
}

static void test_hexdump_line_offset(void) {
    uint8_t data[] = {0xFF};
    char out[128];
    int n = nfs_hexdump_line(data, 1, 0x100, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strncmp(out, "00000100  ff", 12) == 0);
}

static void test_hexdump_line_all_printable(void) {
    uint8_t data[] = "Hello, World!!!"; /* 15 bytes */
    char out[128];
    int n = nfs_hexdump_line(data, 15, 0, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strstr(out, "|Hello, World!!!|") != NULL);
}

/* ---- hexdump (multi-line) tests ---- */

static void test_hexdump_empty(void) {
    char out[128];
    int n = nfs_hexdump(NULL, 0, out, sizeof(out));
    ASSERT_EQ(n, 0);
    ASSERT_EQ(out[0], '\0');
}

static void test_hexdump_single_byte(void) {
    uint8_t data[] = {0xAB};
    char out[128];
    int n = nfs_hexdump(data, 1, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strncmp(out, "00000000  ab", 12) == 0);
}

static void test_hexdump_17_bytes(void) {
    /* 17 bytes should produce exactly 2 lines */
    uint8_t data[17];
    for (int i = 0; i < 17; i++)
        data[i] = (uint8_t)(0x30 + i);
    char out[256];
    int n = nfs_hexdump(data, 17, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    /* First line starts at offset 0 */
    ASSERT_TRUE(strncmp(out, "00000000", 8) == 0);
    /* Second line starts at offset 16 */
    ASSERT_TRUE(strstr(out, "00000010") != NULL);
}

static void test_hexdump_all_zero(void) {
    uint8_t data[16] = {0};
    char out[128];
    int n = nfs_hexdump(data, 16, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strstr(out, "00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00") != NULL);
    /* All zeros are non-printable, ASCII pane should be dots */
    ASSERT_TRUE(strstr(out, "|................|") != NULL);
}

static void test_hexdump_all_ff(void) {
    uint8_t data[16];
    memset(data, 0xFF, 16);
    char out[128];
    int n = nfs_hexdump(data, 16, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strstr(out, "ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff") != NULL);
}

static void test_hexdump_classic_ip_header(void) {
    /* Classic IPv4 header bytes */
    uint8_t data[] = {0x45, 0x00, 0x00, 0x3c, 0x1c, 0x46, 0x40, 0x00,
                      0x40, 0x06, 0xa6, 0xec, 0x0a, 0x00, 0x02, 0x0f};
    char out[128];
    int n = nfs_hexdump(data, 16, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strstr(out, "45 00 00 3c 1c 46 40 00  40 06 a6 ec 0a 00 02 0f") != NULL);
    ASSERT_TRUE(strstr(out, "|E..<.F@.@.......|") != NULL);
}

/* ---- roundtrip tests ---- */

static void test_hex_to_bytes_roundtrip(void) {
    const char *hex = "DEADBEEF01020304";
    uint8_t raw[8];
    int n = nfs_hex_to_bytes(hex, raw, sizeof(raw));
    ASSERT_EQ(n, 8);
    ASSERT_EQ(raw[0], 0xDE);
    ASSERT_EQ(raw[1], 0xAD);
    ASSERT_EQ(raw[7], 0x04);

    /* Dump and verify it contains the hex */
    char out[256];
    int w = nfs_hexdump(raw, (size_t)n, out, sizeof(out));
    ASSERT_TRUE(w > 0);
    ASSERT_TRUE(strstr(out, "de ad be ef 01 02 03 04") != NULL);
}

static void test_hex_to_bytes_mixed_case(void) {
    uint8_t out[3];
    int n = nfs_hex_to_bytes("aAbBcC", out, sizeof(out));
    ASSERT_EQ(n, 3);
    ASSERT_EQ(out[0], 0xAA);
    ASSERT_EQ(out[1], 0xBB);
    ASSERT_EQ(out[2], 0xCC);
}

static void test_hexdump_null_data(void) {
    char out[128];
    int n = nfs_hexdump(NULL, 5, out, sizeof(out));
    ASSERT_EQ(n, -1);
}

static void test_hexdump_null_out(void) {
    uint8_t data[] = {0x41};
    int n = nfs_hexdump(data, 1, NULL, 128);
    ASSERT_EQ(n, -1);
}

static void test_hexdump_line_buffer_too_small(void) {
    uint8_t data[] = {0x41};
    char out[10];
    int n = nfs_hexdump_line(data, 1, 0, out, sizeof(out));
    ASSERT_EQ(n, -1);
}

int main(void) {
    printf("Hex Dump Tests\n");

    test_hex_to_bytes_simple();
    test_hex_to_bytes_with_spaces();
    test_hex_to_bytes_uppercase();
    test_hex_to_bytes_empty();
    test_hex_to_bytes_invalid();
    test_hex_to_bytes_odd_length();
    test_hex_to_bytes_buffer_too_small();
    test_hexdump_line_single_byte();
    test_hexdump_line_16_bytes();
    test_hexdump_line_non_printable();
    test_hexdump_line_offset();
    test_hexdump_line_all_printable();
    test_hexdump_empty();
    test_hexdump_single_byte();
    test_hexdump_17_bytes();
    test_hexdump_all_zero();
    test_hexdump_all_ff();
    test_hexdump_classic_ip_header();
    test_hex_to_bytes_roundtrip();
    test_hex_to_bytes_mixed_case();
    test_hexdump_null_data();
    test_hexdump_null_out();
    test_hexdump_line_buffer_too_small();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
