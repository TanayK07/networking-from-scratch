/* Unit tests for chunked transfer encoding. */

#include "../chunked.h"
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

/* ---- Hex parsing tests ---- */

static void test_parse_hex_basic(void) {
    ASSERT_EQ(nfs_chunked_parse_hex("a", 1), 10);
    ASSERT_EQ(nfs_chunked_parse_hex("1f", 2), 31);
    ASSERT_EQ(nfs_chunked_parse_hex("0", 1), 0);
    ASSERT_EQ(nfs_chunked_parse_hex("ff", 2), 255);
    ASSERT_EQ(nfs_chunked_parse_hex("10", 2), 16);
    ASSERT_EQ(nfs_chunked_parse_hex("FF", 2), 255);
}

static void test_parse_hex_invalid(void) {
    ASSERT_EQ(nfs_chunked_parse_hex("zz", 2), (size_t)-1);
    ASSERT_EQ(nfs_chunked_parse_hex(NULL, 0), (size_t)-1);
}

/* ---- Chunk encoding tests ---- */

static void test_encode_single_chunk(void) {
    const char *data = "hello";
    uint8_t out[64];
    int n = nfs_chunked_encode_chunk((const uint8_t *)data, 5, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    /* Should be "5\r\nhello\r\n" */
    ASSERT_TRUE(memcmp(out, "5\r\nhello\r\n", 10) == 0);
    ASSERT_EQ(n, 10);
}

static void test_encode_last_chunk(void) {
    uint8_t out[64];
    int n = nfs_chunked_encode_last(out, sizeof(out));
    ASSERT_EQ(n, 5);
    ASSERT_TRUE(memcmp(out, "0\r\n\r\n", 5) == 0);
}

static void test_encode_last_chunk_too_small(void) {
    uint8_t out[3];
    ASSERT_EQ(nfs_chunked_encode_last(out, sizeof(out)), -1);
}

static void test_encode_complete(void) {
    const char *data = "Hello, World!"; /* 13 bytes */
    uint8_t out[256];
    int n = nfs_chunked_encode((const uint8_t *)data, 13, 5, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    /* Should have 3 chunks: 5 + 5 + 3, plus terminator */
    /* Verify it ends with "0\r\n\r\n" */
    ASSERT_TRUE(n >= 5);
    ASSERT_TRUE(memcmp(out + n - 5, "0\r\n\r\n", 5) == 0);
}

static void test_encode_null(void) {
    ASSERT_EQ(nfs_chunked_encode(NULL, 10, 5, NULL, 64), -1);
}

static void test_encode_zero_chunk_size(void) {
    uint8_t out[64];
    ASSERT_EQ(nfs_chunked_encode((const uint8_t *)"x", 1, 0, out, sizeof(out)), -1);
}

static void test_encode_empty_data(void) {
    uint8_t out[64];
    int n = nfs_chunked_encode(NULL, 0, 16, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    /* Just the terminator: "0\r\n\r\n" */
    ASSERT_EQ(n, 5);
}

/* ---- Chunk parsing tests ---- */

static void test_parse_chunk_basic(void) {
    const char *raw = "5\r\nhello\r\n";
    struct nfs_chunk chunk;
    int consumed = nfs_chunk_parse_next((const uint8_t *)raw, strlen(raw), &chunk);
    ASSERT_EQ(consumed, 10);
    ASSERT_EQ(chunk.size, 5);
    ASSERT_TRUE(memcmp(chunk.data, "hello", 5) == 0);
}

static void test_parse_chunk_hex_upper(void) {
    const char *raw = "A\r\n0123456789\r\n";
    struct nfs_chunk chunk;
    int consumed = nfs_chunk_parse_next((const uint8_t *)raw, strlen(raw), &chunk);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ(chunk.size, 10);
    ASSERT_TRUE(memcmp(chunk.data, "0123456789", 10) == 0);
}

static void test_parse_last_chunk(void) {
    const char *raw = "0\r\n";
    struct nfs_chunk chunk;
    int consumed = nfs_chunk_parse_next((const uint8_t *)raw, strlen(raw), &chunk);
    ASSERT_EQ(consumed, 3);
    ASSERT_EQ(chunk.size, 0);
    ASSERT_TRUE(chunk.data == NULL);
}

static void test_parse_chunk_null(void) {
    struct nfs_chunk chunk;
    ASSERT_EQ(nfs_chunk_parse_next(NULL, 10, &chunk), -1);
}

static void test_parse_chunk_truncated(void) {
    /* Says 10 bytes but only 3 available */
    const char *raw = "a\r\n123";
    struct nfs_chunk chunk;
    ASSERT_EQ(nfs_chunk_parse_next((const uint8_t *)raw, strlen(raw), &chunk), -1);
}

/* ---- Encode/decode roundtrip tests ---- */

static void test_roundtrip_simple(void) {
    const char *original = "Hello, World!";
    size_t orig_len = strlen(original);

    uint8_t encoded[256];
    int elen = nfs_chunked_encode((const uint8_t *)original, orig_len, 5, encoded, sizeof(encoded));
    ASSERT_TRUE(elen > 0);

    uint8_t decoded_buf[256];
    struct nfs_chunked_decoded decoded;
    decoded.data = decoded_buf;

    int consumed = nfs_chunked_decode(encoded, (size_t)elen, &decoded);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ(decoded.data_len, orig_len);
    ASSERT_TRUE(memcmp(decoded.data, original, orig_len) == 0);
}

static void test_roundtrip_large(void) {
    /* 1000 bytes, chunked at 64 */
    uint8_t original[1000];
    for (int i = 0; i < 1000; i++)
        original[i] = (uint8_t)(i & 0xFF);

    uint8_t encoded[4096];
    int elen = nfs_chunked_encode(original, 1000, 64, encoded, sizeof(encoded));
    ASSERT_TRUE(elen > 0);

    uint8_t decoded_buf[1024];
    struct nfs_chunked_decoded decoded;
    decoded.data = decoded_buf;

    int consumed = nfs_chunked_decode(encoded, (size_t)elen, &decoded);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ(decoded.data_len, 1000);
    ASSERT_TRUE(memcmp(decoded.data, original, 1000) == 0);
}

static void test_roundtrip_single_chunk(void) {
    const char *data = "one chunk";
    size_t dlen = strlen(data);

    uint8_t encoded[256];
    int elen = nfs_chunked_encode((const uint8_t *)data, dlen, 100, encoded, sizeof(encoded));
    ASSERT_TRUE(elen > 0);

    uint8_t decoded_buf[256];
    struct nfs_chunked_decoded decoded;
    decoded.data = decoded_buf;

    nfs_chunked_decode(encoded, (size_t)elen, &decoded);
    ASSERT_EQ(decoded.data_len, dlen);
    ASSERT_TRUE(memcmp(decoded.data, data, dlen) == 0);
}

/* ---- Decode with trailers ---- */

static void test_decode_with_trailers(void) {
    /* Manually build chunked data with trailers */
    const char *raw = "5\r\nhello\r\n"
                      "0\r\n"
                      "Checksum: abc123\r\n"
                      "\r\n";

    uint8_t decoded_buf[256];
    struct nfs_chunked_decoded decoded;
    decoded.data = decoded_buf;

    int consumed = nfs_chunked_decode((const uint8_t *)raw, strlen(raw), &decoded);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ(decoded.data_len, 5);
    ASSERT_TRUE(memcmp(decoded.data, "hello", 5) == 0);
    ASSERT_EQ(decoded.trailer_count, 1);
    ASSERT_TRUE(strcmp(decoded.trailers[0].name, "Checksum") == 0);
    ASSERT_TRUE(strcmp(decoded.trailers[0].value, "abc123") == 0);
}

static void test_decode_null(void) {
    uint8_t buf[64];
    struct nfs_chunked_decoded decoded;
    decoded.data = buf;
    ASSERT_EQ(nfs_chunked_decode(NULL, 10, &decoded), -1);
}

static void test_decode_multi_chunk_manual(void) {
    const char *raw = "4\r\nWiki\r\n"
                      "5\r\npedia\r\n"
                      "0\r\n"
                      "\r\n";

    uint8_t decoded_buf[256];
    struct nfs_chunked_decoded decoded;
    decoded.data = decoded_buf;

    int consumed = nfs_chunked_decode((const uint8_t *)raw, strlen(raw), &decoded);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ(decoded.data_len, 9);
    ASSERT_TRUE(memcmp(decoded.data, "Wikipedia", 9) == 0);
}

int main(void) {
    printf("Chunked Transfer Encoding Tests\n");

    test_parse_hex_basic();
    test_parse_hex_invalid();
    test_encode_single_chunk();
    test_encode_last_chunk();
    test_encode_last_chunk_too_small();
    test_encode_complete();
    test_encode_null();
    test_encode_zero_chunk_size();
    test_encode_empty_data();
    test_parse_chunk_basic();
    test_parse_chunk_hex_upper();
    test_parse_last_chunk();
    test_parse_chunk_null();
    test_parse_chunk_truncated();
    test_roundtrip_simple();
    test_roundtrip_large();
    test_roundtrip_single_chunk();
    test_decode_with_trailers();
    test_decode_null();
    test_decode_multi_chunk_manual();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
