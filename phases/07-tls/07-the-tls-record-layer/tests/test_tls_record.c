/* Unit tests for TLS Record Layer. */

#include "../tls_record.h"
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

/* ---- Record build/parse roundtrip ---- */

static void test_build_parse_handshake(void) {
    uint8_t buf[64];
    const uint8_t frag[] = {0x01, 0x00, 0x00, 0x05};
    int n =
        nfs_tls_record_build(buf, sizeof(buf), NFS_TLS_CT_HANDSHAKE, NFS_TLS_VERSION_12, frag, 4);
    ASSERT_EQ(n, 9); /* 5 header + 4 fragment */

    struct nfs_tls_record rec;
    int consumed = nfs_tls_record_parse(buf, (size_t)n, &rec);
    ASSERT_EQ(consumed, 9);
    ASSERT_EQ(rec.content_type, NFS_TLS_CT_HANDSHAKE);
    ASSERT_EQ(rec.version, NFS_TLS_VERSION_12);
    ASSERT_EQ(rec.length, 4);
    ASSERT_TRUE(memcmp(rec.fragment, frag, 4) == 0);
}

static void test_build_parse_app_data(void) {
    uint8_t buf[128];
    const uint8_t data[] = "Hello, TLS!";
    int n = nfs_tls_record_build(buf, sizeof(buf), NFS_TLS_CT_APPLICATION, NFS_TLS_VERSION_12, data,
                                 11);
    ASSERT_EQ(n, 16);

    struct nfs_tls_record rec;
    ASSERT_TRUE(nfs_tls_record_parse(buf, (size_t)n, &rec) > 0);
    ASSERT_EQ(rec.content_type, NFS_TLS_CT_APPLICATION);
    ASSERT_EQ(rec.length, 11);
}

static void test_build_empty_fragment(void) {
    uint8_t buf[16];
    int n = nfs_tls_record_build(buf, sizeof(buf), NFS_TLS_CT_CHANGE_CIPHER, NFS_TLS_VERSION_12,
                                 NULL, 0);
    ASSERT_EQ(n, 5);

    struct nfs_tls_record rec;
    ASSERT_TRUE(nfs_tls_record_parse(buf, (size_t)n, &rec) > 0);
    ASSERT_EQ(rec.length, 0);
}

/* ---- Wire format verification ---- */

static void test_wire_format(void) {
    uint8_t buf[16];
    const uint8_t frag[] = {0xAB};
    nfs_tls_record_build(buf, sizeof(buf), NFS_TLS_CT_ALERT, NFS_TLS_VERSION_12, frag, 1);

    /* content_type = 21 */
    ASSERT_EQ(buf[0], 0x15);
    /* version = 0x0303 (TLS 1.2) */
    ASSERT_EQ(buf[1], 0x03);
    ASSERT_EQ(buf[2], 0x03);
    /* length = 1 in big-endian */
    ASSERT_EQ(buf[3], 0x00);
    ASSERT_EQ(buf[4], 0x01);
    /* fragment */
    ASSERT_EQ(buf[5], 0xAB);
}

static void test_parse_tls10_version(void) {
    /* TLS 1.0 handshake record: 0x16 0x03 0x01 0x00 0x02 + 2 bytes */
    uint8_t buf[] = {0x16, 0x03, 0x01, 0x00, 0x02, 0x01, 0x00};
    struct nfs_tls_record rec;
    ASSERT_TRUE(nfs_tls_record_parse(buf, sizeof(buf), &rec) > 0);
    ASSERT_EQ(rec.version, NFS_TLS_VERSION_10);
    ASSERT_EQ(rec.content_type, NFS_TLS_CT_HANDSHAKE);
}

/* ---- Parse error handling ---- */

static void test_parse_too_short(void) {
    uint8_t buf[] = {0x16, 0x03, 0x03, 0x00};
    struct nfs_tls_record rec;
    ASSERT_EQ(nfs_tls_record_parse(buf, 4, &rec), -1);
}

static void test_parse_truncated_fragment(void) {
    /* Header says length=10 but only 5 bytes of fragment */
    uint8_t buf[] = {0x16, 0x03, 0x03, 0x00, 0x0A, 0x01, 0x02, 0x03, 0x04, 0x05};
    struct nfs_tls_record rec;
    ASSERT_EQ(nfs_tls_record_parse(buf, sizeof(buf), &rec), -1);
}

static void test_parse_invalid_content_type(void) {
    uint8_t buf[] = {0x00, 0x03, 0x03, 0x00, 0x01, 0xFF};
    struct nfs_tls_record rec;
    ASSERT_EQ(nfs_tls_record_parse(buf, sizeof(buf), &rec), -1);
}

static void test_parse_null(void) {
    struct nfs_tls_record rec;
    ASSERT_EQ(nfs_tls_record_parse(NULL, 10, &rec), -1);
    uint8_t buf[10] = {0};
    ASSERT_EQ(nfs_tls_record_parse(buf, 10, NULL), -1);
}

static void test_build_null(void) {
    ASSERT_EQ(nfs_tls_record_build(NULL, 64, NFS_TLS_CT_HANDSHAKE, NFS_TLS_VERSION_12, NULL, 0),
              -1);
}

static void test_build_buffer_too_small(void) {
    uint8_t buf[4];
    ASSERT_EQ(
        nfs_tls_record_build(buf, sizeof(buf), NFS_TLS_CT_HANDSHAKE, NFS_TLS_VERSION_12, NULL, 0),
        -1);
}

/* ---- Alert tests ---- */

static void test_alert_build_parse(void) {
    uint8_t frag[2];
    int n = nfs_tls_alert_build(frag, sizeof(frag), NFS_TLS_ALERT_FATAL,
                                NFS_TLS_ALERT_HANDSHAKE_FAILURE);
    ASSERT_EQ(n, 2);
    ASSERT_EQ(frag[0], 2);  /* fatal */
    ASSERT_EQ(frag[1], 40); /* handshake_failure */

    struct nfs_tls_alert alert;
    ASSERT_EQ(nfs_tls_alert_parse(frag, 2, &alert), 0);
    ASSERT_EQ(alert.level, NFS_TLS_ALERT_FATAL);
    ASSERT_EQ(alert.description, NFS_TLS_ALERT_HANDSHAKE_FAILURE);
}

static void test_alert_warning(void) {
    uint8_t frag[2];
    nfs_tls_alert_build(frag, sizeof(frag), NFS_TLS_ALERT_WARNING, NFS_TLS_ALERT_CLOSE_NOTIFY);

    struct nfs_tls_alert alert;
    ASSERT_EQ(nfs_tls_alert_parse(frag, 2, &alert), 0);
    ASSERT_EQ(alert.level, NFS_TLS_ALERT_WARNING);
    ASSERT_EQ(alert.description, NFS_TLS_ALERT_CLOSE_NOTIFY);
}

static void test_alert_in_record(void) {
    uint8_t record[16];
    uint8_t frag[2];
    nfs_tls_alert_build(frag, sizeof(frag), NFS_TLS_ALERT_FATAL, NFS_TLS_ALERT_BAD_RECORD_MAC);
    int n =
        nfs_tls_record_build(record, sizeof(record), NFS_TLS_CT_ALERT, NFS_TLS_VERSION_12, frag, 2);
    ASSERT_EQ(n, 7);

    struct nfs_tls_record rec;
    ASSERT_TRUE(nfs_tls_record_parse(record, (size_t)n, &rec) > 0);
    ASSERT_EQ(rec.content_type, NFS_TLS_CT_ALERT);

    struct nfs_tls_alert alert;
    ASSERT_EQ(nfs_tls_alert_parse(rec.fragment, rec.length, &alert), 0);
    ASSERT_EQ(alert.description, NFS_TLS_ALERT_BAD_RECORD_MAC);
}

static void test_alert_parse_too_short(void) {
    uint8_t frag[] = {0x02};
    struct nfs_tls_alert alert;
    ASSERT_EQ(nfs_tls_alert_parse(frag, 1, &alert), -1);
}

static void test_alert_parse_invalid_level(void) {
    uint8_t frag[] = {0x03, 0x00}; /* level 3 is invalid */
    struct nfs_tls_alert alert;
    ASSERT_EQ(nfs_tls_alert_parse(frag, 2, &alert), -1);
}

static void test_alert_parse_null(void) {
    struct nfs_tls_alert alert;
    ASSERT_EQ(nfs_tls_alert_parse(NULL, 2, &alert), -1);
}

/* ---- Name lookup tests ---- */

static void test_content_type_names(void) {
    ASSERT_TRUE(strcmp(nfs_tls_content_type_name(NFS_TLS_CT_CHANGE_CIPHER), "change_cipher_spec") ==
                0);
    ASSERT_TRUE(strcmp(nfs_tls_content_type_name(NFS_TLS_CT_ALERT), "alert") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_content_type_name(NFS_TLS_CT_HANDSHAKE), "handshake") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_content_type_name(NFS_TLS_CT_APPLICATION), "application_data") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_content_type_name(0), "unknown") == 0);
}

static void test_version_names(void) {
    ASSERT_TRUE(strcmp(nfs_tls_version_name(NFS_TLS_VERSION_10), "TLS 1.0") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_version_name(NFS_TLS_VERSION_12), "TLS 1.2") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_version_name(NFS_TLS_VERSION_13), "TLS 1.3") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_version_name(0x9999), "unknown") == 0);
}

static void test_alert_names(void) {
    ASSERT_TRUE(strcmp(nfs_tls_alert_desc_name(NFS_TLS_ALERT_CLOSE_NOTIFY), "close_notify") == 0);
    ASSERT_TRUE(
        strcmp(nfs_tls_alert_desc_name(NFS_TLS_ALERT_HANDSHAKE_FAILURE), "handshake_failure") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_alert_desc_name(255), "unknown") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_alert_level_name(NFS_TLS_ALERT_WARNING), "warning") == 0);
    ASSERT_TRUE(strcmp(nfs_tls_alert_level_name(NFS_TLS_ALERT_FATAL), "fatal") == 0);
}

int main(void) {
    printf("TLS Record Layer Tests\n");

    test_build_parse_handshake();
    test_build_parse_app_data();
    test_build_empty_fragment();
    test_wire_format();
    test_parse_tls10_version();
    test_parse_too_short();
    test_parse_truncated_fragment();
    test_parse_invalid_content_type();
    test_parse_null();
    test_build_null();
    test_build_buffer_too_small();
    test_alert_build_parse();
    test_alert_warning();
    test_alert_in_record();
    test_alert_parse_too_short();
    test_alert_parse_invalid_level();
    test_alert_parse_null();
    test_content_type_names();
    test_version_names();
    test_alert_names();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
