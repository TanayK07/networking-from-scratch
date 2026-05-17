/* Unit tests for Syslog RFC 5424. */

#include "../syslog5424.h"
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

/* ---- PRI encoding/decoding ---- */

static void test_pri_encode_kern_emerg(void) {
    ASSERT_EQ(nfs_syslog_pri_encode(NFS_SYSLOG_KERN, NFS_SYSLOG_EMERG), 0);
}

static void test_pri_encode_user_info(void) {
    /* user(1) * 8 + info(6) = 14 */
    ASSERT_EQ(nfs_syslog_pri_encode(NFS_SYSLOG_USER, NFS_SYSLOG_INFO), 14);
}

static void test_pri_encode_local0_debug(void) {
    /* local0(16) * 8 + debug(7) = 135 */
    ASSERT_EQ(nfs_syslog_pri_encode(NFS_SYSLOG_LOCAL0, NFS_SYSLOG_DEBUG), 135);
}

static void test_pri_encode_local7_emerg(void) {
    /* local7(23) * 8 + emerg(0) = 184 */
    ASSERT_EQ(nfs_syslog_pri_encode(NFS_SYSLOG_LOCAL7, NFS_SYSLOG_EMERG), 184);
}

static void test_pri_encode_invalid(void) {
    ASSERT_EQ(nfs_syslog_pri_encode(24, 0), -1);
    ASSERT_EQ(nfs_syslog_pri_encode(0, 8), -1);
}

static void test_pri_decode(void) {
    uint8_t fac, sev;
    ASSERT_EQ(nfs_syslog_pri_decode(14, &fac, &sev), 0);
    ASSERT_EQ(fac, NFS_SYSLOG_USER);
    ASSERT_EQ(sev, NFS_SYSLOG_INFO);
}

static void test_pri_decode_max(void) {
    uint8_t fac, sev;
    ASSERT_EQ(nfs_syslog_pri_decode(191, &fac, &sev), 0);
    ASSERT_EQ(fac, 23);
    ASSERT_EQ(sev, 7);
}

static void test_pri_decode_invalid(void) {
    uint8_t fac, sev;
    ASSERT_EQ(nfs_syslog_pri_decode(192, &fac, &sev), -1);
    ASSERT_EQ(nfs_syslog_pri_decode(-1, &fac, &sev), -1);
}

/* ---- Build/parse roundtrip ---- */

static void test_build_parse_full(void) {
    struct nfs_syslog_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.facility = NFS_SYSLOG_LOCAL0;
    msg.severity = NFS_SYSLOG_INFO;
    msg.version = 1;
    strncpy(msg.timestamp, "2023-11-14T12:00:00Z", sizeof(msg.timestamp) - 1);
    strncpy(msg.hostname, "myhost", sizeof(msg.hostname) - 1);
    strncpy(msg.app_name, "myapp", sizeof(msg.app_name) - 1);
    strncpy(msg.procid, "1234", sizeof(msg.procid) - 1);
    strncpy(msg.msgid, "ID47", sizeof(msg.msgid) - 1);
    strncpy(msg.msg, "Test message", sizeof(msg.msg) - 1);

    char buf[4096];
    int n = nfs_syslog_build(buf, sizeof(buf), &msg);
    ASSERT_TRUE(n > 0);

    struct nfs_syslog_msg out;
    ASSERT_EQ(nfs_syslog_parse(buf, (size_t)n, &out), 0);
    ASSERT_EQ(out.facility, NFS_SYSLOG_LOCAL0);
    ASSERT_EQ(out.severity, NFS_SYSLOG_INFO);
    ASSERT_EQ(out.version, 1);
    ASSERT_TRUE(strcmp(out.timestamp, "2023-11-14T12:00:00Z") == 0);
    ASSERT_TRUE(strcmp(out.hostname, "myhost") == 0);
    ASSERT_TRUE(strcmp(out.app_name, "myapp") == 0);
    ASSERT_TRUE(strcmp(out.procid, "1234") == 0);
    ASSERT_TRUE(strcmp(out.msgid, "ID47") == 0);
    ASSERT_TRUE(strcmp(out.msg, "Test message") == 0);
}

static void test_build_parse_nil_fields(void) {
    struct nfs_syslog_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.facility = NFS_SYSLOG_KERN;
    msg.severity = NFS_SYSLOG_EMERG;
    msg.version = 1;
    /* All other fields left empty -> "-" */

    char buf[4096];
    int n = nfs_syslog_build(buf, sizeof(buf), &msg);
    ASSERT_TRUE(n > 0);
    /* Should contain: <0>1 - - - - - - */
    ASSERT_TRUE(strstr(buf, "<0>1 - - - - - -") != NULL);

    struct nfs_syslog_msg out;
    ASSERT_EQ(nfs_syslog_parse(buf, (size_t)n, &out), 0);
    ASSERT_EQ(out.facility, NFS_SYSLOG_KERN);
    ASSERT_EQ(out.severity, NFS_SYSLOG_EMERG);
    ASSERT_TRUE(out.hostname[0] == '\0');
    ASSERT_TRUE(out.app_name[0] == '\0');
}

static void test_build_parse_with_sd(void) {
    struct nfs_syslog_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.facility = NFS_SYSLOG_AUTH;
    msg.severity = NFS_SYSLOG_WARNING;
    msg.version = 1;
    strncpy(msg.timestamp, "2023-01-01T00:00:00Z", sizeof(msg.timestamp) - 1);
    strncpy(msg.hostname, "fw", sizeof(msg.hostname) - 1);
    strncpy(msg.app_name, "sshd", sizeof(msg.app_name) - 1);
    strncpy(msg.sd, "[origin ip=\"10.0.0.1\"]", sizeof(msg.sd) - 1);
    strncpy(msg.msg, "Login failed", sizeof(msg.msg) - 1);

    char buf[4096];
    int n = nfs_syslog_build(buf, sizeof(buf), &msg);
    ASSERT_TRUE(n > 0);

    struct nfs_syslog_msg out;
    ASSERT_EQ(nfs_syslog_parse(buf, (size_t)n, &out), 0);
    ASSERT_TRUE(strcmp(out.sd, "[origin ip=\"10.0.0.1\"]") == 0);
    ASSERT_TRUE(strcmp(out.msg, "Login failed") == 0);
}

/* ---- Facility/severity names ---- */

static void test_facility_names(void) {
    ASSERT_TRUE(strcmp(nfs_syslog_facility_name(NFS_SYSLOG_KERN), "kern") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_facility_name(NFS_SYSLOG_USER), "user") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_facility_name(NFS_SYSLOG_MAIL), "mail") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_facility_name(NFS_SYSLOG_DAEMON), "daemon") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_facility_name(NFS_SYSLOG_AUTH), "auth") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_facility_name(NFS_SYSLOG_SYSLOG), "syslog") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_facility_name(NFS_SYSLOG_LOCAL0), "local0") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_facility_name(NFS_SYSLOG_LOCAL7), "local7") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_facility_name(99), "unknown") == 0);
}

static void test_severity_names(void) {
    ASSERT_TRUE(strcmp(nfs_syslog_severity_name(NFS_SYSLOG_EMERG), "emerg") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_severity_name(NFS_SYSLOG_ALERT_SEV), "alert") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_severity_name(NFS_SYSLOG_CRIT), "crit") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_severity_name(NFS_SYSLOG_ERR), "err") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_severity_name(NFS_SYSLOG_WARNING), "warning") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_severity_name(NFS_SYSLOG_NOTICE), "notice") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_severity_name(NFS_SYSLOG_INFO), "info") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_severity_name(NFS_SYSLOG_DEBUG), "debug") == 0);
    ASSERT_TRUE(strcmp(nfs_syslog_severity_name(99), "unknown") == 0);
}

/* ---- Error cases ---- */

static void test_parse_null(void) {
    struct nfs_syslog_msg out;
    ASSERT_EQ(nfs_syslog_parse(NULL, 10, &out), -1);
}

static void test_parse_empty(void) {
    struct nfs_syslog_msg out;
    ASSERT_EQ(nfs_syslog_parse("", 0, &out), -1);
}

static void test_parse_no_pri(void) {
    struct nfs_syslog_msg out;
    ASSERT_EQ(nfs_syslog_parse("no PRI here", 11, &out), -1);
}

static void test_build_null(void) {
    ASSERT_EQ(nfs_syslog_build(NULL, 100, NULL), -1);
}

static void test_parse_known_message(void) {
    /* Manually crafted RFC 5424 message */
    const char *raw = "<34>1 2003-10-11T22:14:15.003Z mymachine su - ID47 - 'su root' failed";
    struct nfs_syslog_msg out;
    ASSERT_EQ(nfs_syslog_parse(raw, strlen(raw), &out), 0);
    ASSERT_EQ(out.facility, NFS_SYSLOG_AUTH); /* 34/8 = 4 */
    ASSERT_EQ(out.severity, NFS_SYSLOG_CRIT); /* 34%8 = 2 */
    ASSERT_EQ(out.version, 1);
    ASSERT_TRUE(strcmp(out.hostname, "mymachine") == 0);
    ASSERT_TRUE(strcmp(out.app_name, "su") == 0);
    ASSERT_TRUE(strcmp(out.msgid, "ID47") == 0);
    ASSERT_TRUE(strstr(out.msg, "'su root' failed") != NULL);
}

int main(void) {
    printf("Syslog RFC 5424 Tests\n");

    test_pri_encode_kern_emerg();
    test_pri_encode_user_info();
    test_pri_encode_local0_debug();
    test_pri_encode_local7_emerg();
    test_pri_encode_invalid();
    test_pri_decode();
    test_pri_decode_max();
    test_pri_decode_invalid();
    test_build_parse_full();
    test_build_parse_nil_fields();
    test_build_parse_with_sd();
    test_facility_names();
    test_severity_names();
    test_parse_null();
    test_parse_empty();
    test_parse_no_pri();
    test_build_null();
    test_parse_known_message();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
