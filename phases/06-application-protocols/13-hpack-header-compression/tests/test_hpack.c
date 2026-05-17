/* Unit tests for HPACK header compression. */

#include "../hpack.h"
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

/* ---- Integer encoding tests (RFC 7541 examples) ---- */

static void test_encode_10_5bit(void) {
    /* RFC 7541 C.1.1: encode 10 with 5-bit prefix -> 0x0a */
    uint8_t buf[8];
    int n = nfs_hpack_encode_int(buf, sizeof(buf), 10, 5);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(buf[0], 0x0a);
}

static void test_encode_1337_5bit(void) {
    /* RFC 7541 C.1.2: encode 1337 with 5-bit prefix -> 0x1f 0x9a 0x0a */
    uint8_t buf[8];
    int n = nfs_hpack_encode_int(buf, sizeof(buf), 1337, 5);
    ASSERT_EQ(n, 3);
    ASSERT_EQ(buf[0], 0x1f);
    ASSERT_EQ(buf[1], 0x9a);
    ASSERT_EQ(buf[2], 0x0a);
}

static void test_encode_42_8bit(void) {
    /* 42 with 8-bit prefix -> single byte 0x2a */
    uint8_t buf[8];
    int n = nfs_hpack_encode_int(buf, sizeof(buf), 42, 8);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(buf[0], 42);
}

static void test_decode_10_5bit(void) {
    uint8_t buf[] = {0x0a};
    uint64_t val;
    int n = nfs_hpack_decode_int(buf, 1, &val, 5);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(val, 10);
}

static void test_decode_1337_5bit(void) {
    uint8_t buf[] = {0x1f, 0x9a, 0x0a};
    uint64_t val;
    int n = nfs_hpack_decode_int(buf, 3, &val, 5);
    ASSERT_EQ(n, 3);
    ASSERT_EQ(val, 1337);
}

static void test_int_roundtrip_various(void) {
    uint64_t test_values[] = {0, 1, 30, 31, 127, 128, 255, 256, 1337, 65535, 100000};
    uint8_t prefixes[] = {5, 6, 7};

    for (size_t p = 0; p < sizeof(prefixes); p++) {
        for (size_t v = 0; v < sizeof(test_values) / sizeof(test_values[0]); v++) {
            uint8_t buf[16];
            int enc = nfs_hpack_encode_int(buf, sizeof(buf), test_values[v], prefixes[p]);
            ASSERT_TRUE(enc > 0);

            uint64_t decoded;
            int dec = nfs_hpack_decode_int(buf, (size_t)enc, &decoded, prefixes[p]);
            ASSERT_EQ(dec, enc);
            ASSERT_EQ(decoded, (long long)test_values[v]);
        }
    }
}

static void test_decode_truncated(void) {
    /* Multi-byte but buffer ends before continuation completes */
    uint8_t buf[] = {0x1f, 0x9a}; /* 1337 needs 3 bytes */
    uint64_t val;
    ASSERT_EQ(nfs_hpack_decode_int(buf, 2, &val, 5), -1);
}

static void test_encode_buffer_too_small(void) {
    uint8_t buf[1];
    /* 1337 needs 3 bytes but buffer is 1 */
    ASSERT_EQ(nfs_hpack_encode_int(buf, 1, 1337, 5), -1);
}

static void test_encode_null(void) {
    ASSERT_EQ(nfs_hpack_encode_int(NULL, 10, 5, 5), -1);
}

static void test_decode_null(void) {
    uint64_t val;
    ASSERT_EQ(nfs_hpack_decode_int(NULL, 10, &val, 5), -1);
}

/* ---- Static table tests ---- */

static void test_static_lookup_authority(void) {
    const char *name, *value;
    ASSERT_EQ(nfs_hpack_static_lookup(1, &name, &value), 0);
    ASSERT_TRUE(strcmp(name, ":authority") == 0);
    ASSERT_TRUE(strcmp(value, "") == 0);
}

static void test_static_lookup_method_get(void) {
    const char *name, *value;
    ASSERT_EQ(nfs_hpack_static_lookup(2, &name, &value), 0);
    ASSERT_TRUE(strcmp(name, ":method") == 0);
    ASSERT_TRUE(strcmp(value, "GET") == 0);
}

static void test_static_lookup_status_200(void) {
    const char *name, *value;
    ASSERT_EQ(nfs_hpack_static_lookup(8, &name, &value), 0);
    ASSERT_TRUE(strcmp(name, ":status") == 0);
    ASSERT_TRUE(strcmp(value, "200") == 0);
}

static void test_static_lookup_last(void) {
    const char *name, *value;
    ASSERT_EQ(nfs_hpack_static_lookup(61, &name, &value), 0);
    ASSERT_TRUE(strcmp(name, "www-authenticate") == 0);
}

static void test_static_lookup_out_of_range(void) {
    const char *name, *value;
    ASSERT_EQ(nfs_hpack_static_lookup(0, &name, &value), -1);
    ASSERT_EQ(nfs_hpack_static_lookup(62, &name, &value), -1);
}

static void test_static_find_method_get(void) {
    int idx = nfs_hpack_static_find(":method", "GET");
    ASSERT_EQ(idx, 2);
}

static void test_static_find_method_post(void) {
    int idx = nfs_hpack_static_find(":method", "POST");
    ASSERT_EQ(idx, 3);
}

static void test_static_find_name_only(void) {
    int idx = nfs_hpack_static_find("content-type", NULL);
    ASSERT_EQ(idx, 31);
}

static void test_static_find_not_found(void) {
    int idx = nfs_hpack_static_find("x-custom", "val");
    ASSERT_EQ(idx, 0);
}

/* ---- Dynamic table tests ---- */

static void test_dyn_init_empty(void) {
    struct nfs_hpack_dynamic_table dt;
    nfs_hpack_dyn_init(&dt, 4096);
    ASSERT_EQ(nfs_hpack_dyn_count(&dt), 0);
}

static void test_dyn_add_single(void) {
    struct nfs_hpack_dynamic_table dt;
    nfs_hpack_dyn_init(&dt, 4096);
    ASSERT_EQ(nfs_hpack_dyn_add(&dt, "custom", "value"), 0);
    ASSERT_EQ(nfs_hpack_dyn_count(&dt), 1);

    const char *name, *value;
    ASSERT_EQ(nfs_hpack_dyn_lookup(&dt, 0, &name, &value), 0);
    ASSERT_TRUE(strcmp(name, "custom") == 0);
    ASSERT_TRUE(strcmp(value, "value") == 0);
}

static void test_dyn_add_multiple_fifo(void) {
    struct nfs_hpack_dynamic_table dt;
    nfs_hpack_dyn_init(&dt, 4096);
    nfs_hpack_dyn_add(&dt, "first", "1");
    nfs_hpack_dyn_add(&dt, "second", "2");
    nfs_hpack_dyn_add(&dt, "third", "3");

    ASSERT_EQ(nfs_hpack_dyn_count(&dt), 3);

    /* Index 0 = newest */
    const char *name, *value;
    ASSERT_EQ(nfs_hpack_dyn_lookup(&dt, 0, &name, &value), 0);
    ASSERT_TRUE(strcmp(name, "third") == 0);

    ASSERT_EQ(nfs_hpack_dyn_lookup(&dt, 2, &name, &value), 0);
    ASSERT_TRUE(strcmp(name, "first") == 0);
}

static void test_dyn_eviction(void) {
    struct nfs_hpack_dynamic_table dt;
    /* Each entry is ~32 + name_len + value_len bytes
       "aaa" + "bbb" = 6 + 32 = 38 bytes.
       Max size 80: fits 2 entries, third evicts first. */
    nfs_hpack_dyn_init(&dt, 80);
    nfs_hpack_dyn_add(&dt, "aaa", "bbb"); /* 38 bytes */
    nfs_hpack_dyn_add(&dt, "ccc", "ddd"); /* 38 bytes, total 76 */
    ASSERT_EQ(nfs_hpack_dyn_count(&dt), 2);

    nfs_hpack_dyn_add(&dt, "eee", "fff"); /* 38 bytes, evicts "aaa" */
    ASSERT_EQ(nfs_hpack_dyn_count(&dt), 2);

    const char *name, *value;
    /* Oldest should now be "ccc" */
    ASSERT_EQ(nfs_hpack_dyn_lookup(&dt, 1, &name, &value), 0);
    ASSERT_TRUE(strcmp(name, "ccc") == 0);

    /* Newest should be "eee" */
    ASSERT_EQ(nfs_hpack_dyn_lookup(&dt, 0, &name, &value), 0);
    ASSERT_TRUE(strcmp(name, "eee") == 0);
}

static void test_dyn_oversized_entry(void) {
    struct nfs_hpack_dynamic_table dt;
    nfs_hpack_dyn_init(&dt, 40);
    nfs_hpack_dyn_add(&dt, "aaa", "bbb"); /* 38 bytes, fits */
    ASSERT_EQ(nfs_hpack_dyn_count(&dt), 1);

    /* This entry is way too large: 32 + 100 + 100 = 232 > 40 */
    char big[101];
    memset(big, 'X', 100);
    big[100] = '\0';
    nfs_hpack_dyn_add(&dt, big, big); /* evicts everything, adds nothing */
    ASSERT_EQ(nfs_hpack_dyn_count(&dt), 0);
}

static void test_dyn_lookup_out_of_range(void) {
    struct nfs_hpack_dynamic_table dt;
    nfs_hpack_dyn_init(&dt, 4096);
    nfs_hpack_dyn_add(&dt, "x", "y");

    const char *name, *value;
    ASSERT_EQ(nfs_hpack_dyn_lookup(&dt, 1, &name, &value), -1);
    ASSERT_EQ(nfs_hpack_dyn_lookup(&dt, -1, &name, &value), -1);
}

int main(void) {
    printf("HPACK Header Compression Tests\n");

    test_encode_10_5bit();
    test_encode_1337_5bit();
    test_encode_42_8bit();
    test_decode_10_5bit();
    test_decode_1337_5bit();
    test_int_roundtrip_various();
    test_decode_truncated();
    test_encode_buffer_too_small();
    test_encode_null();
    test_decode_null();
    test_static_lookup_authority();
    test_static_lookup_method_get();
    test_static_lookup_status_200();
    test_static_lookup_last();
    test_static_lookup_out_of_range();
    test_static_find_method_get();
    test_static_find_method_post();
    test_static_find_name_only();
    test_static_find_not_found();
    test_dyn_init_empty();
    test_dyn_add_single();
    test_dyn_add_multiple_fifo();
    test_dyn_eviction();
    test_dyn_oversized_entry();
    test_dyn_lookup_out_of_range();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
