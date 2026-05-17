/* Unit tests for Scatter/Gather I/O. */

#include "../iovec.h"
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

/* ---- Total length ---- */

static void test_total_len_single(void) {
    uint8_t buf[10];
    struct nfs_iovec iov = {buf, 10};
    ASSERT_EQ(nfs_iov_total_len(&iov, 1), 10);
}

static void test_total_len_multiple(void) {
    uint8_t b1[5], b2[15], b3[30];
    struct nfs_iovec iov[3] = {{b1, 5}, {b2, 15}, {b3, 30}};
    ASSERT_EQ(nfs_iov_total_len(iov, 3), 50);
}

static void test_total_len_empty(void) {
    ASSERT_EQ(nfs_iov_total_len(NULL, 0), 0);
}

static void test_total_len_zero_iovcnt(void) {
    uint8_t b[10];
    struct nfs_iovec iov = {b, 10};
    ASSERT_EQ(nfs_iov_total_len(&iov, 0), 0);
}

/* ---- Scatter ---- */

static void test_scatter_exact_fit(void) {
    uint8_t b1[5], b2[5];
    struct nfs_iovec iov[2] = {{b1, 5}, {b2, 5}};

    const uint8_t data[] = "0123456789";
    int n = nfs_iov_scatter(iov, 2, data, 10);
    ASSERT_EQ(n, 10);
    ASSERT_TRUE(memcmp(b1, "01234", 5) == 0);
    ASSERT_TRUE(memcmp(b2, "56789", 5) == 0);
}

static void test_scatter_partial(void) {
    uint8_t b1[3], b2[3];
    struct nfs_iovec iov[2] = {{b1, 3}, {b2, 3}};

    /* 10 bytes into 6 bytes of buffer space -> only 6 copied */
    const uint8_t data[] = "0123456789";
    int n = nfs_iov_scatter(iov, 2, data, 10);
    ASSERT_EQ(n, 6);
    ASSERT_TRUE(memcmp(b1, "012", 3) == 0);
    ASSERT_TRUE(memcmp(b2, "345", 3) == 0);
}

static void test_scatter_single_buffer(void) {
    uint8_t buf[20];
    struct nfs_iovec iov = {buf, 20};

    const uint8_t data[] = "hello";
    int n = nfs_iov_scatter(&iov, 1, data, 5);
    ASSERT_EQ(n, 5);
    ASSERT_TRUE(memcmp(buf, "hello", 5) == 0);
}

static void test_scatter_empty_data(void) {
    uint8_t buf[10];
    struct nfs_iovec iov = {buf, 10};
    int n = nfs_iov_scatter(&iov, 1, (const uint8_t *)"", 0);
    ASSERT_EQ(n, 0);
}

static void test_scatter_null(void) {
    ASSERT_EQ(nfs_iov_scatter(NULL, 1, (const uint8_t *)"x", 1), -1);
}

/* ---- Gather ---- */

static void test_gather_exact(void) {
    uint8_t b1[] = "Hello";
    uint8_t b2[] = "World";
    struct nfs_iovec iov[2] = {{b1, 5}, {b2, 5}};

    uint8_t out[10];
    int n = nfs_iov_gather(iov, 2, out, sizeof(out));
    ASSERT_EQ(n, 10);
    ASSERT_TRUE(memcmp(out, "HelloWorld", 10) == 0);
}

static void test_gather_single(void) {
    uint8_t data[] = "test";
    struct nfs_iovec iov = {data, 4};

    uint8_t out[10];
    int n = nfs_iov_gather(&iov, 1, out, sizeof(out));
    ASSERT_EQ(n, 4);
    ASSERT_TRUE(memcmp(out, "test", 4) == 0);
}

static void test_gather_buffer_too_small(void) {
    uint8_t b1[] = "Large";
    struct nfs_iovec iov = {b1, 5};

    uint8_t out[3]; /* too small */
    ASSERT_EQ(nfs_iov_gather(&iov, 1, out, sizeof(out)), -1);
}

static void test_gather_null(void) {
    ASSERT_EQ(nfs_iov_gather(NULL, 1, NULL, 0), -1);
}

/* ---- Scatter/gather roundtrip ---- */

static void test_scatter_gather_roundtrip(void) {
    uint8_t b1[4], b2[4], b3[4];
    struct nfs_iovec iov[3] = {{b1, 4}, {b2, 4}, {b3, 4}};

    const uint8_t data[] = "ABCDEFGHIJKL";
    nfs_iov_scatter(iov, 3, data, 12);

    uint8_t out[12];
    int n = nfs_iov_gather(iov, 3, out, sizeof(out));
    ASSERT_EQ(n, 12);
    ASSERT_TRUE(memcmp(out, data, 12) == 0);
}

static void test_scatter_gather_large(void) {
    uint8_t b1[100], b2[200], b3[300];
    struct nfs_iovec iov[3] = {{b1, 100}, {b2, 200}, {b3, 300}};

    uint8_t data[600];
    for (int i = 0; i < 600; i++)
        data[i] = (uint8_t)(i & 0xFF);

    nfs_iov_scatter(iov, 3, data, 600);

    uint8_t out[600];
    int n = nfs_iov_gather(iov, 3, out, sizeof(out));
    ASSERT_EQ(n, 600);
    ASSERT_TRUE(memcmp(out, data, 600) == 0);
}

/* ---- Format ---- */

static void test_format_single(void) {
    uint8_t buf[10];
    struct nfs_iovec iov = {buf, 10};

    char out[64];
    int n = nfs_iov_format(&iov, 1, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strcmp(out, "[0] 10 bytes") == 0);
}

static void test_format_multiple(void) {
    uint8_t b1[5], b2[15];
    struct nfs_iovec iov[2] = {{b1, 5}, {b2, 15}};

    char out[64];
    nfs_iov_format(iov, 2, out, sizeof(out));
    ASSERT_TRUE(strstr(out, "[0] 5 bytes") != NULL);
    ASSERT_TRUE(strstr(out, "[1] 15 bytes") != NULL);
}

static void test_format_null(void) {
    ASSERT_EQ(nfs_iov_format(NULL, 1, NULL, 0), -1);
}

/* ---- Find byte ---- */

static void test_find_byte_first_iov(void) {
    uint8_t b1[5], b2[5];
    struct nfs_iovec iov[2] = {{b1, 5}, {b2, 5}};

    int idx;
    size_t off;
    ASSERT_EQ(nfs_iov_find_byte(iov, 2, 3, &idx, &off), 0);
    ASSERT_EQ(idx, 0);
    ASSERT_EQ(off, 3);
}

static void test_find_byte_second_iov(void) {
    uint8_t b1[5], b2[5];
    struct nfs_iovec iov[2] = {{b1, 5}, {b2, 5}};

    int idx;
    size_t off;
    ASSERT_EQ(nfs_iov_find_byte(iov, 2, 7, &idx, &off), 0);
    ASSERT_EQ(idx, 1);
    ASSERT_EQ(off, 2);
}

static void test_find_byte_boundary(void) {
    uint8_t b1[5], b2[5];
    struct nfs_iovec iov[2] = {{b1, 5}, {b2, 5}};

    int idx;
    size_t off;
    /* Byte 5 = first byte of second iov */
    ASSERT_EQ(nfs_iov_find_byte(iov, 2, 5, &idx, &off), 0);
    ASSERT_EQ(idx, 1);
    ASSERT_EQ(off, 0);
}

static void test_find_byte_last(void) {
    uint8_t b1[5], b2[5];
    struct nfs_iovec iov[2] = {{b1, 5}, {b2, 5}};

    int idx;
    size_t off;
    /* Byte 9 = last byte */
    ASSERT_EQ(nfs_iov_find_byte(iov, 2, 9, &idx, &off), 0);
    ASSERT_EQ(idx, 1);
    ASSERT_EQ(off, 4);
}

static void test_find_byte_out_of_range(void) {
    uint8_t b1[5];
    struct nfs_iovec iov = {b1, 5};

    int idx;
    size_t off;
    ASSERT_EQ(nfs_iov_find_byte(&iov, 1, 5, &idx, &off), -1);
    ASSERT_EQ(nfs_iov_find_byte(&iov, 1, 100, &idx, &off), -1);
}

static void test_find_byte_null(void) {
    int idx;
    size_t off;
    ASSERT_EQ(nfs_iov_find_byte(NULL, 1, 0, &idx, &off), -1);
}

int main(void) {
    printf("Scatter/Gather I/O Tests\n");

    test_total_len_single();
    test_total_len_multiple();
    test_total_len_empty();
    test_total_len_zero_iovcnt();
    test_scatter_exact_fit();
    test_scatter_partial();
    test_scatter_single_buffer();
    test_scatter_empty_data();
    test_scatter_null();
    test_gather_exact();
    test_gather_single();
    test_gather_buffer_too_small();
    test_gather_null();
    test_scatter_gather_roundtrip();
    test_scatter_gather_large();
    test_format_single();
    test_format_multiple();
    test_format_null();
    test_find_byte_first_iov();
    test_find_byte_second_iov();
    test_find_byte_boundary();
    test_find_byte_last();
    test_find_byte_out_of_range();
    test_find_byte_null();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
