/* Unit tests for TLS 1.3 ClientHello. */

#include "../tls_clienthello.h"
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

/* Helper: build a minimal ClientHello for testing. */
static int build_test_ch(uint8_t *buf, size_t buf_max) {
    struct nfs_tls_client_hello ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_version = 0x0303;
    for (int i = 0; i < 32; i++)
        ch.random[i] = (uint8_t)i;
    ch.session_id_len = 0;
    ch.cipher_suites_count = 2;
    ch.cipher_suites[0] = NFS_TLS_CS_AES_128_GCM_SHA256;
    ch.cipher_suites[1] = NFS_TLS_CS_AES_256_GCM_SHA384;
    ch.compression_len = 1;
    ch.compression[0] = 0x00;

    /* supported_versions extension */
    uint8_t sv[] = {0x03, 0x03, 0x04};
    ch.extensions_count = 1;
    ch.extensions[0].type = NFS_TLS_EXT_SUPPORTED_VERSIONS;
    ch.extensions[0].length = 3;
    const uint8_t *ext_bufs[] = {sv};

    return nfs_tls_ch_build(buf, buf_max, &ch, ext_bufs);
}

/* ---- Build/Parse roundtrip ---- */

static void test_roundtrip_basic(void) {
    uint8_t buf[512];
    int n = build_test_ch(buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    struct nfs_tls_client_hello parsed;
    int consumed = nfs_tls_ch_parse(buf, (size_t)n, &parsed);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ(parsed.msg_type, NFS_TLS_HS_CLIENT_HELLO);
    ASSERT_EQ(parsed.legacy_version, 0x0303);
    ASSERT_EQ(parsed.random[0], 0x00);
    ASSERT_EQ(parsed.random[31], 31);
    ASSERT_EQ(parsed.session_id_len, 0);
    ASSERT_EQ(parsed.cipher_suites_count, 2);
    ASSERT_EQ(parsed.cipher_suites[0], NFS_TLS_CS_AES_128_GCM_SHA256);
    ASSERT_EQ(parsed.cipher_suites[1], NFS_TLS_CS_AES_256_GCM_SHA384);
    ASSERT_EQ(parsed.compression_len, 1);
    ASSERT_EQ(parsed.compression[0], 0x00);
    ASSERT_EQ(parsed.extensions_count, 1);
    ASSERT_EQ(parsed.extensions[0].type, NFS_TLS_EXT_SUPPORTED_VERSIONS);
}

/* ---- Handshake header wire format ---- */

static void test_handshake_header(void) {
    uint8_t buf[512];
    int n = build_test_ch(buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    /* First byte = msg_type = 0x01 (ClientHello) */
    ASSERT_EQ(buf[0], 0x01);

    /* Next 3 bytes = length (big-endian 24-bit) */
    uint32_t body_len = ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3];
    ASSERT_EQ((int)(body_len + 4), n);
}

/* ---- Session ID ---- */

static void test_with_session_id(void) {
    struct nfs_tls_client_hello ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_version = 0x0303;
    memset(ch.random, 0xAA, 32);
    ch.session_id_len = 4;
    ch.session_id[0] = 0xDE;
    ch.session_id[1] = 0xAD;
    ch.session_id[2] = 0xBE;
    ch.session_id[3] = 0xEF;
    ch.cipher_suites_count = 1;
    ch.cipher_suites[0] = NFS_TLS_CS_AES_128_GCM_SHA256;
    ch.compression_len = 1;
    ch.compression[0] = 0x00;
    ch.extensions_count = 0;

    uint8_t buf[512];
    int n = nfs_tls_ch_build(buf, sizeof(buf), &ch, NULL);
    ASSERT_TRUE(n > 0);

    struct nfs_tls_client_hello parsed;
    ASSERT_TRUE(nfs_tls_ch_parse(buf, (size_t)n, &parsed) > 0);
    ASSERT_EQ(parsed.session_id_len, 4);
    ASSERT_EQ(parsed.session_id[0], 0xDE);
    ASSERT_EQ(parsed.session_id[3], 0xEF);
}

/* ---- Multiple cipher suites ---- */

static void test_three_cipher_suites(void) {
    struct nfs_tls_client_hello ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_version = 0x0303;
    memset(ch.random, 0, 32);
    ch.session_id_len = 0;
    ch.cipher_suites_count = 3;
    ch.cipher_suites[0] = NFS_TLS_CS_AES_128_GCM_SHA256;
    ch.cipher_suites[1] = NFS_TLS_CS_AES_256_GCM_SHA384;
    ch.cipher_suites[2] = NFS_TLS_CS_CHACHA20_POLY1305_SHA256;
    ch.compression_len = 1;
    ch.compression[0] = 0x00;
    ch.extensions_count = 0;

    uint8_t buf[512];
    int n = nfs_tls_ch_build(buf, sizeof(buf), &ch, NULL);
    ASSERT_TRUE(n > 0);

    struct nfs_tls_client_hello parsed;
    ASSERT_TRUE(nfs_tls_ch_parse(buf, (size_t)n, &parsed) > 0);
    ASSERT_EQ(parsed.cipher_suites_count, 3);
    ASSERT_EQ(parsed.cipher_suites[2], NFS_TLS_CS_CHACHA20_POLY1305_SHA256);
}

/* ---- Multiple extensions ---- */

static void test_multiple_extensions(void) {
    struct nfs_tls_client_hello ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_version = 0x0303;
    memset(ch.random, 0, 32);
    ch.session_id_len = 0;
    ch.cipher_suites_count = 1;
    ch.cipher_suites[0] = NFS_TLS_CS_AES_128_GCM_SHA256;
    ch.compression_len = 1;
    ch.compression[0] = 0x00;

    /* Two extensions */
    uint8_t sv[] = {0x03, 0x03, 0x04};
    uint8_t ks[] = {0x00, 0x1D, 0x00, 0x20}; /* simplified key_share */

    ch.extensions_count = 2;
    ch.extensions[0].type = NFS_TLS_EXT_SUPPORTED_VERSIONS;
    ch.extensions[0].length = sizeof(sv);
    ch.extensions[1].type = NFS_TLS_EXT_KEY_SHARE;
    ch.extensions[1].length = sizeof(ks);

    const uint8_t *ext_bufs[] = {sv, ks};

    uint8_t buf[512];
    int n = nfs_tls_ch_build(buf, sizeof(buf), &ch, ext_bufs);
    ASSERT_TRUE(n > 0);

    struct nfs_tls_client_hello parsed;
    ASSERT_TRUE(nfs_tls_ch_parse(buf, (size_t)n, &parsed) > 0);
    ASSERT_EQ(parsed.extensions_count, 2);
    ASSERT_EQ(parsed.extensions[0].type, NFS_TLS_EXT_SUPPORTED_VERSIONS);
    ASSERT_EQ(parsed.extensions[0].length, 3);
    ASSERT_EQ(parsed.extensions[1].type, NFS_TLS_EXT_KEY_SHARE);
    ASSERT_EQ(parsed.extensions[1].length, 4);
}

/* ---- Extension iterator ---- */

static void test_ext_iterator(void) {
    /* Build raw extension data: type(2)+len(2)+data for two extensions */
    uint8_t raw[] = {
        0x00, 0x2B, 0x00, 0x02, 0xAA, 0xBB, /* supported_versions, 2 bytes */
        0x00, 0x33, 0x00, 0x01, 0xCC        /* key_share, 1 byte */
    };

    struct nfs_tls_ext_iter it;
    nfs_tls_ext_iter_init(&it, raw, sizeof(raw));

    struct nfs_tls_extension ext;
    ASSERT_EQ(nfs_tls_ext_iter_next(&it, &ext), 0);
    ASSERT_EQ(ext.type, NFS_TLS_EXT_SUPPORTED_VERSIONS);
    ASSERT_EQ(ext.length, 2);
    ASSERT_EQ(ext.data[0], 0xAA);

    ASSERT_EQ(nfs_tls_ext_iter_next(&it, &ext), 0);
    ASSERT_EQ(ext.type, NFS_TLS_EXT_KEY_SHARE);
    ASSERT_EQ(ext.length, 1);
    ASSERT_EQ(ext.data[0], 0xCC);

    /* End of data */
    ASSERT_EQ(nfs_tls_ext_iter_next(&it, &ext), -1);
}

static void test_ext_iterator_empty(void) {
    struct nfs_tls_ext_iter it;
    nfs_tls_ext_iter_init(&it, NULL, 0);

    struct nfs_tls_extension ext;
    ASSERT_EQ(nfs_tls_ext_iter_next(&it, &ext), -1);
}

/* ---- Parse error handling ---- */

static void test_parse_too_short(void) {
    uint8_t buf[] = {0x01, 0x00, 0x00};
    struct nfs_tls_client_hello ch;
    ASSERT_EQ(nfs_tls_ch_parse(buf, 3, &ch), -1);
}

static void test_parse_wrong_msg_type(void) {
    uint8_t buf[512];
    int n = build_test_ch(buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    buf[0] = 0x02; /* ServerHello instead of ClientHello */
    struct nfs_tls_client_hello ch;
    ASSERT_EQ(nfs_tls_ch_parse(buf, (size_t)n, &ch), -1);
}

static void test_parse_null(void) {
    struct nfs_tls_client_hello ch;
    ASSERT_EQ(nfs_tls_ch_parse(NULL, 64, &ch), -1);
    uint8_t buf[4] = {0};
    ASSERT_EQ(nfs_tls_ch_parse(buf, 4, NULL), -1);
}

static void test_build_null(void) {
    ASSERT_EQ(nfs_tls_ch_build(NULL, 512, NULL, NULL), -1);
}

static void test_build_buffer_too_small(void) {
    uint8_t buf[4];
    struct nfs_tls_client_hello ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_version = 0x0303;
    ch.cipher_suites_count = 1;
    ch.cipher_suites[0] = 0x1301;
    ch.compression_len = 1;
    ASSERT_EQ(nfs_tls_ch_build(buf, sizeof(buf), &ch, NULL), -1);
}

/* ---- Cipher suite names ---- */

static void test_cipher_suite_names(void) {
    ASSERT_TRUE(strcmp(nfs_tls_cipher_suite_name(NFS_TLS_CS_AES_128_GCM_SHA256),
                       "TLS_AES_128_GCM_SHA256") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_cipher_suite_name(NFS_TLS_CS_AES_256_GCM_SHA384),
                       "TLS_AES_256_GCM_SHA384") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_cipher_suite_name(NFS_TLS_CS_CHACHA20_POLY1305_SHA256),
                       "TLS_CHACHA20_POLY1305_SHA256") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_cipher_suite_name(0x9999), "UNKNOWN") == 0);
}

/* ---- Extension names ---- */

static void test_extension_names(void) {
    ASSERT_TRUE(strcmp(nfs_tls_extension_name(NFS_TLS_EXT_SERVER_NAME), "server_name") == 0);
    ASSERT_TRUE(
        strcmp(nfs_tls_extension_name(NFS_TLS_EXT_SUPPORTED_VERSIONS), "supported_versions") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_extension_name(NFS_TLS_EXT_KEY_SHARE), "key_share") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_extension_name(NFS_TLS_EXT_SIGNATURE_ALGORITHMS),
                       "signature_algorithms") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_extension_name(0xFFFF), "UNKNOWN") == 0);
}

/* ---- Byte-level wire verification ---- */

static void test_cipher_suite_wire_bytes(void) {
    struct nfs_tls_client_hello ch;
    memset(&ch, 0, sizeof(ch));
    ch.legacy_version = 0x0303;
    memset(ch.random, 0, 32);
    ch.session_id_len = 0;
    ch.cipher_suites_count = 1;
    ch.cipher_suites[0] = 0x1301;
    ch.compression_len = 1;
    ch.compression[0] = 0x00;
    ch.extensions_count = 0;

    uint8_t buf[512];
    int n = nfs_tls_ch_build(buf, sizeof(buf), &ch, NULL);
    ASSERT_TRUE(n > 0);

    /* Cipher suites start after: hs_hdr(4) + version(2) + random(32) + sid_len(1) = 39 */
    /* cs_len(2) = 0x00 0x02, then suite 0x13 0x01 */
    ASSERT_EQ(buf[39], 0x00);
    ASSERT_EQ(buf[40], 0x02);
    ASSERT_EQ(buf[41], 0x13);
    ASSERT_EQ(buf[42], 0x01);
}

int main(void) {
    printf("TLS 1.3 ClientHello Tests\n");

    test_roundtrip_basic();
    test_handshake_header();
    test_with_session_id();
    test_three_cipher_suites();
    test_multiple_extensions();
    test_ext_iterator();
    test_ext_iterator_empty();
    test_parse_too_short();
    test_parse_wrong_msg_type();
    test_parse_null();
    test_build_null();
    test_build_buffer_too_small();
    test_cipher_suite_names();
    test_extension_names();
    test_cipher_suite_wire_bytes();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
