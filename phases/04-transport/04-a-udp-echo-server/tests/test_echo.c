/* Unit tests for UDP Echo Server (RFC 862). */

#include "../echo.h"
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

/* ---- Init tests ---- */

static void test_init_default(void) {
    struct nfs_echo_server srv;
    ASSERT_EQ(nfs_echo_init(&srv, 0), 0);
    ASSERT_EQ(srv.running, 1);
    ASSERT_EQ(srv.max_payload, NFS_ECHO_MAX_PAYLOAD);
    ASSERT_EQ(srv.stats.packets_processed, 0);
    ASSERT_EQ(srv.stats.bytes_echoed, 0);
    ASSERT_EQ(srv.stats.errors, 0);
}

static void test_init_custom_max(void) {
    struct nfs_echo_server srv;
    ASSERT_EQ(nfs_echo_init(&srv, 512), 0);
    ASSERT_EQ(srv.max_payload, 512);
}

static void test_init_null(void) {
    ASSERT_EQ(nfs_echo_init(NULL, 0), -1);
}

/* ---- Echo process tests ---- */

static void test_echo_simple(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 0);

    const uint8_t in[] = "Hello";
    uint8_t out[64];
    size_t out_len = 0;

    ASSERT_EQ(nfs_echo_process(&srv, in, 5, out, sizeof(out), &out_len), 0);
    ASSERT_EQ(out_len, 5);
    ASSERT_TRUE(memcmp(out, "Hello", 5) == 0);
}

static void test_echo_empty_payload(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 0);

    uint8_t out[64];
    size_t out_len = 99;

    ASSERT_EQ(nfs_echo_process(&srv, (const uint8_t *)"", 0, out, sizeof(out), &out_len), 0);
    ASSERT_EQ(out_len, 0);
}

static void test_echo_binary_data(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 0);

    uint8_t in[256];
    for (int i = 0; i < 256; i++)
        in[i] = (uint8_t)i;

    uint8_t out[256];
    size_t out_len = 0;

    ASSERT_EQ(nfs_echo_process(&srv, in, 256, out, sizeof(out), &out_len), 0);
    ASSERT_EQ(out_len, 256);
    ASSERT_TRUE(memcmp(out, in, 256) == 0);
}

static void test_echo_exact_max_payload(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 100);

    uint8_t in[100];
    memset(in, 0xAB, 100);
    uint8_t out[100];
    size_t out_len = 0;

    ASSERT_EQ(nfs_echo_process(&srv, in, 100, out, sizeof(out), &out_len), 0);
    ASSERT_EQ(out_len, 100);
}

static void test_echo_exceeds_max_payload(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 100);

    uint8_t in[101];
    memset(in, 0xAB, 101);
    uint8_t out[101];
    size_t out_len = 0;

    ASSERT_EQ(nfs_echo_process(&srv, in, 101, out, sizeof(out), &out_len), -1);
}

static void test_echo_output_too_small(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 0);

    const uint8_t in[] = "Hello, World!";
    uint8_t out[5];
    size_t out_len = 0;

    ASSERT_EQ(nfs_echo_process(&srv, in, 13, out, sizeof(out), &out_len), -1);
}

static void test_echo_null_input_nonzero_len(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 0);

    uint8_t out[64];
    size_t out_len = 0;

    ASSERT_EQ(nfs_echo_process(&srv, NULL, 10, out, sizeof(out), &out_len), -1);
}

static void test_echo_null_output(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 0);

    const uint8_t in[] = "test";
    size_t out_len = 0;

    ASSERT_EQ(nfs_echo_process(&srv, in, 4, NULL, 64, &out_len), -1);
}

static void test_echo_null_out_len(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 0);

    const uint8_t in[] = "test";
    uint8_t out[64];

    ASSERT_EQ(nfs_echo_process(&srv, in, 4, out, sizeof(out), NULL), -1);
}

static void test_echo_not_initialized(void) {
    struct nfs_echo_server srv;
    memset(&srv, 0, sizeof(srv)); /* running = 0 */

    const uint8_t in[] = "test";
    uint8_t out[64];
    size_t out_len = 0;

    ASSERT_EQ(nfs_echo_process(&srv, in, 4, out, sizeof(out), &out_len), -1);
}

/* ---- Stats tests ---- */

static void test_stats_after_echoes(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 0);

    uint8_t out[64];
    size_t out_len;

    nfs_echo_process(&srv, (const uint8_t *)"aaa", 3, out, sizeof(out), &out_len);
    nfs_echo_process(&srv, (const uint8_t *)"bb", 2, out, sizeof(out), &out_len);
    nfs_echo_process(&srv, (const uint8_t *)"c", 1, out, sizeof(out), &out_len);

    struct nfs_echo_stats stats;
    ASSERT_EQ(nfs_echo_stats(&srv, &stats), 0);
    ASSERT_EQ(stats.packets_processed, 3);
    ASSERT_EQ(stats.bytes_echoed, 6);
    ASSERT_EQ(stats.errors, 0);
}

static void test_stats_with_errors(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 10);

    uint8_t out[64];
    size_t out_len;

    /* Successful echo */
    nfs_echo_process(&srv, (const uint8_t *)"ok", 2, out, sizeof(out), &out_len);
    /* Error: too large */
    uint8_t big[20];
    memset(big, 0, 20);
    nfs_echo_process(&srv, big, 20, out, sizeof(out), &out_len);

    struct nfs_echo_stats stats;
    nfs_echo_stats(&srv, &stats);
    ASSERT_EQ(stats.packets_processed, 1);
    ASSERT_EQ(stats.bytes_echoed, 2);
    ASSERT_EQ(stats.errors, 1);
}

static void test_stats_null(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 0);

    ASSERT_EQ(nfs_echo_stats(NULL, &srv.stats), -1);
    ASSERT_EQ(nfs_echo_stats(&srv, NULL), -1);
}

static void test_reset_stats(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 0);

    uint8_t out[64];
    size_t out_len;
    nfs_echo_process(&srv, (const uint8_t *)"test", 4, out, sizeof(out), &out_len);

    ASSERT_EQ(nfs_echo_reset_stats(&srv), 0);

    struct nfs_echo_stats stats;
    nfs_echo_stats(&srv, &stats);
    ASSERT_EQ(stats.packets_processed, 0);
    ASSERT_EQ(stats.bytes_echoed, 0);
    ASSERT_EQ(stats.errors, 0);
}

/* ---- Shutdown tests ---- */

static void test_shutdown(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 0);
    ASSERT_EQ(nfs_echo_shutdown(&srv), 0);
    ASSERT_EQ(srv.running, 0);

    /* Should fail after shutdown */
    uint8_t out[64];
    size_t out_len;
    ASSERT_EQ(nfs_echo_process(&srv, (const uint8_t *)"x", 1, out, sizeof(out), &out_len), -1);
}

static void test_shutdown_null(void) {
    ASSERT_EQ(nfs_echo_shutdown(NULL), -1);
}

/* ---- Data integrity tests ---- */

static void test_echo_preserves_all_bytes(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 0);

    /* Payload with NUL bytes, high bytes, etc */
    uint8_t in[] = {0x00, 0xFF, 0x01, 0xFE, 0x80, 0x7F, 0x00, 0x00};
    uint8_t out[8];
    size_t out_len;

    ASSERT_EQ(nfs_echo_process(&srv, in, 8, out, sizeof(out), &out_len), 0);
    ASSERT_EQ(out_len, 8);
    ASSERT_TRUE(memcmp(out, in, 8) == 0);
}

static void test_echo_single_byte(void) {
    struct nfs_echo_server srv;
    nfs_echo_init(&srv, 0);

    uint8_t in = 0x42;
    uint8_t out = 0;
    size_t out_len;

    ASSERT_EQ(nfs_echo_process(&srv, &in, 1, &out, 1, &out_len), 0);
    ASSERT_EQ(out_len, 1);
    ASSERT_EQ(out, 0x42);
}

int main(void) {
    printf("UDP Echo Server Tests\n");

    test_init_default();
    test_init_custom_max();
    test_init_null();
    test_echo_simple();
    test_echo_empty_payload();
    test_echo_binary_data();
    test_echo_exact_max_payload();
    test_echo_exceeds_max_payload();
    test_echo_output_too_small();
    test_echo_null_input_nonzero_len();
    test_echo_null_output();
    test_echo_null_out_len();
    test_echo_not_initialized();
    test_stats_after_echoes();
    test_stats_with_errors();
    test_stats_null();
    test_reset_stats();
    test_shutdown();
    test_shutdown_null();
    test_echo_preserves_all_bytes();
    test_echo_single_byte();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
