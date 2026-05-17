/* Unit tests for nfs_wscale — TCP Window Scaling Option (RFC 7323). */

#include "../wscale.h"
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

/* ---- apply tests ---- */

static void test_apply_shift_0(void) {
    ASSERT_EQ(nfs_wscale_apply(65535, 0), 65535);
    ASSERT_EQ(nfs_wscale_apply(1, 0), 1);
    ASSERT_EQ(nfs_wscale_apply(0, 0), 0);
}

static void test_apply_shift_1(void) {
    ASSERT_EQ(nfs_wscale_apply(100, 1), 200);
    ASSERT_EQ(nfs_wscale_apply(65535, 1), 131070);
}

static void test_apply_shift_7(void) {
    /* shift=7 means multiply by 128 */
    ASSERT_EQ(nfs_wscale_apply(1, 7), 128);
    ASSERT_EQ(nfs_wscale_apply(512, 7), 65536);
    ASSERT_EQ(nfs_wscale_apply(65535, 7), 8388480);
}

static void test_apply_shift_14(void) {
    /* shift=14 means multiply by 16384 */
    ASSERT_EQ(nfs_wscale_apply(1, 14), 16384);
    ASSERT_EQ(nfs_wscale_apply(65535, 14), 1073725440);
}

static void test_apply_max_window(void) {
    /* Maximum effective window: 65535 << 14 = 1073725440 (just under 1 GiB) */
    uint32_t max = nfs_wscale_apply(65535, 14);
    ASSERT_TRUE(max <= 0x3FFFC000); /* 2^30 - 16384 */
    ASSERT_EQ(max, 65535u * 16384u);
}

/* ---- compress tests ---- */

static void test_compress_shift_0(void) {
    ASSERT_EQ(nfs_wscale_compress(65535, 0), 65535);
    ASSERT_EQ(nfs_wscale_compress(100, 0), 100);
}

static void test_compress_shift_7(void) {
    /* 8388480 >> 7 = 65535 */
    ASSERT_EQ(nfs_wscale_compress(8388480, 7), 65535);
    ASSERT_EQ(nfs_wscale_compress(128, 7), 1);
}

static void test_compress_roundtrip(void) {
    /* compress(apply(w, s), s) should give back w (for exact values) */
    for (uint8_t s = 0; s <= NFS_WSCALE_MAX; s++) {
        uint16_t w = 1000;
        uint32_t eff = nfs_wscale_apply(w, s);
        uint16_t back = nfs_wscale_compress(eff, s);
        ASSERT_EQ(back, w);
    }
}

static void test_compress_large_value_clamped(void) {
    /* If real_window >> shift > 0xFFFF, should clamp to 0xFFFF */
    ASSERT_EQ(nfs_wscale_compress(0xFFFFFFFF, 0), 0xFFFF);
}

/* ---- compute tests ---- */

static void test_compute_fits_16bit(void) {
    ASSERT_EQ(nfs_wscale_compute(0), 0);
    ASSERT_EQ(nfs_wscale_compute(1), 0);
    ASSERT_EQ(nfs_wscale_compute(65535), 0);
}

static void test_compute_needs_shift_1(void) {
    ASSERT_EQ(nfs_wscale_compute(65536), 1);
    ASSERT_EQ(nfs_wscale_compute(131070), 1);
}

static void test_compute_needs_shift_7(void) {
    /* 65535 << 7 = 8388480.  A window of 8388480 needs shift=7. */
    uint8_t s = nfs_wscale_compute(8388480);
    ASSERT_EQ(s, 7);
}

static void test_compute_large_window(void) {
    /* 1 GiB = 1073741824 needs shift=14 */
    uint8_t s = nfs_wscale_compute(1073741824);
    ASSERT_EQ(s, 14);
}

static void test_compute_max_shift_cap(void) {
    /* Even for enormous values, shift should not exceed 14 */
    uint8_t s = nfs_wscale_compute(0xFFFFFFFF);
    ASSERT_TRUE(s <= NFS_WSCALE_MAX);
}

/* ---- build option tests ---- */

static void test_build_option_basic(void) {
    uint8_t opt[4];
    int n = nfs_wscale_build_option(7, opt, sizeof(opt));
    ASSERT_EQ(n, 3);
    ASSERT_EQ(opt[0], 3); /* kind */
    ASSERT_EQ(opt[1], 3); /* length */
    ASSERT_EQ(opt[2], 7); /* shift */
}

static void test_build_option_shift_0(void) {
    uint8_t opt[3];
    int n = nfs_wscale_build_option(0, opt, sizeof(opt));
    ASSERT_EQ(n, 3);
    ASSERT_EQ(opt[2], 0);
}

static void test_build_option_shift_14(void) {
    uint8_t opt[3];
    int n = nfs_wscale_build_option(14, opt, sizeof(opt));
    ASSERT_EQ(n, 3);
    ASSERT_EQ(opt[2], 14);
}

static void test_build_option_invalid_shift(void) {
    uint8_t opt[3];
    int n = nfs_wscale_build_option(15, opt, sizeof(opt));
    ASSERT_EQ(n, -1);
}

static void test_build_option_buffer_too_small(void) {
    uint8_t opt[2];
    int n = nfs_wscale_build_option(7, opt, sizeof(opt));
    ASSERT_EQ(n, -1);
}

/* ---- parse option tests ---- */

static void test_parse_option_basic(void) {
    uint8_t data[] = {3, 3, 10};
    uint8_t shift;
    int rc = nfs_wscale_parse_option(data, sizeof(data), &shift);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(shift, 10);
}

static void test_parse_option_wrong_kind(void) {
    uint8_t data[] = {4, 3, 7};
    uint8_t shift;
    int rc = nfs_wscale_parse_option(data, sizeof(data), &shift);
    ASSERT_EQ(rc, -1);
}

static void test_parse_option_wrong_len(void) {
    uint8_t data[] = {3, 4, 7};
    uint8_t shift;
    int rc = nfs_wscale_parse_option(data, sizeof(data), &shift);
    ASSERT_EQ(rc, -1);
}

static void test_parse_option_too_short(void) {
    uint8_t data[] = {3, 3};
    uint8_t shift;
    int rc = nfs_wscale_parse_option(data, 2, &shift);
    ASSERT_EQ(rc, -1);
}

static void test_build_parse_roundtrip(void) {
    for (uint8_t s = 0; s <= NFS_WSCALE_MAX; s++) {
        uint8_t opt[3];
        int n = nfs_wscale_build_option(s, opt, sizeof(opt));
        ASSERT_EQ(n, 3);

        uint8_t parsed;
        int rc = nfs_wscale_parse_option(opt, (size_t)n, &parsed);
        ASSERT_EQ(rc, 0);
        ASSERT_EQ(parsed, s);
    }
}

/* ---- valid tests ---- */

static void test_valid_shifts(void) {
    for (uint8_t s = 0; s <= 14; s++) {
        ASSERT_EQ(nfs_wscale_valid(s), 1);
    }
    ASSERT_EQ(nfs_wscale_valid(15), 0);
    ASSERT_EQ(nfs_wscale_valid(255), 0);
}

/* ---- format tests ---- */

static void test_format_shift_0(void) {
    char buf[64];
    nfs_wscale_format(0, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "WScale=0") != NULL);
    ASSERT_TRUE(strstr(buf, "multiply by 1") != NULL);
}

static void test_format_shift_7(void) {
    char buf[64];
    nfs_wscale_format(7, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "WScale=7") != NULL);
    ASSERT_TRUE(strstr(buf, "multiply by 128") != NULL);
}

static void test_format_invalid(void) {
    char buf[64];
    nfs_wscale_format(15, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "INVALID") != NULL);
}

/* ---- effective window size tests ---- */

static void test_effective_window_common_sizes(void) {
    /* Shift=7 with header window=512 => 65536 bytes */
    ASSERT_EQ(nfs_wscale_apply(512, 7), 65536);
    /* Shift=8 with header window=256 => 65536 bytes */
    ASSERT_EQ(nfs_wscale_apply(256, 8), 65536);
}

int main(void) {
    printf("Window Scale Tests\n");

    test_apply_shift_0();
    test_apply_shift_1();
    test_apply_shift_7();
    test_apply_shift_14();
    test_apply_max_window();
    test_compress_shift_0();
    test_compress_shift_7();
    test_compress_roundtrip();
    test_compress_large_value_clamped();
    test_compute_fits_16bit();
    test_compute_needs_shift_1();
    test_compute_needs_shift_7();
    test_compute_large_window();
    test_compute_max_shift_cap();
    test_build_option_basic();
    test_build_option_shift_0();
    test_build_option_shift_14();
    test_build_option_invalid_shift();
    test_build_option_buffer_too_small();
    test_parse_option_basic();
    test_parse_option_wrong_kind();
    test_parse_option_wrong_len();
    test_parse_option_too_short();
    test_build_parse_roundtrip();
    test_valid_shifts();
    test_format_shift_0();
    test_format_shift_7();
    test_format_invalid();
    test_effective_window_common_sizes();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
