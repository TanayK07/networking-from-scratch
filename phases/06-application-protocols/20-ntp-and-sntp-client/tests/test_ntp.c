/* Unit tests for NTP / SNTP client. */

#include "../ntp.h"
#include <math.h>
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

#define ASSERT_NEAR(expr, expected, eps)                                                           \
    do {                                                                                           \
        tests_run++;                                                                               \
        double _got = (double)(expr);                                                              \
        double _exp = (double)(expected);                                                          \
        if (fabs(_got - _exp) > (eps)) {                                                           \
            fprintf(stderr, "  FAIL %s:%d: %s == %f, want %f (eps=%f)\n", __FILE__, __LINE__,      \
                    #expr, _got, _exp, (double)(eps));                                             \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

/* ---- Build/parse roundtrip ---- */

static void test_client_request_roundtrip(void) {
    struct nfs_ntp_packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.li = NFS_NTP_LI_NONE;
    pkt.vn = 4;
    pkt.mode = NFS_NTP_MODE_CLIENT;
    pkt.xmit_ts.seconds = 3908908800U;  /* ~2023-11-14 */
    pkt.xmit_ts.fraction = 0x80000000U; /* 0.5 seconds */

    uint8_t buf[64];
    int n = nfs_ntp_build(buf, sizeof(buf), &pkt);
    ASSERT_EQ(n, 48);

    struct nfs_ntp_packet out;
    ASSERT_EQ(nfs_ntp_parse(buf, (size_t)n, &out), 0);
    ASSERT_EQ(out.li, NFS_NTP_LI_NONE);
    ASSERT_EQ(out.vn, 4);
    ASSERT_EQ(out.mode, NFS_NTP_MODE_CLIENT);
    ASSERT_EQ(out.xmit_ts.seconds, 3908908800U);
    ASSERT_EQ(out.xmit_ts.fraction, 0x80000000U);
}

static void test_full_packet_roundtrip(void) {
    struct nfs_ntp_packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.li = NFS_NTP_LI_LAST61;
    pkt.vn = 3;
    pkt.mode = NFS_NTP_MODE_SERVER;
    pkt.stratum = 2;
    pkt.poll = 6;
    pkt.precision = -20;
    pkt.root_delay = 0x00010000;      /* 1.0 seconds in fixed-point */
    pkt.root_dispersion = 0x00008000; /* 0.5 seconds */
    pkt.ref_id = 0x47505300;          /* "GPS\0" */
    pkt.ref_ts.seconds = 3900000000U;
    pkt.ref_ts.fraction = 0;
    pkt.origin_ts.seconds = 3900000001U;
    pkt.recv_ts.seconds = 3900000002U;
    pkt.xmit_ts.seconds = 3900000003U;

    uint8_t buf[64];
    ASSERT_EQ(nfs_ntp_build(buf, sizeof(buf), &pkt), 48);

    struct nfs_ntp_packet out;
    ASSERT_EQ(nfs_ntp_parse(buf, 48, &out), 0);
    ASSERT_EQ(out.li, NFS_NTP_LI_LAST61);
    ASSERT_EQ(out.vn, 3);
    ASSERT_EQ(out.mode, NFS_NTP_MODE_SERVER);
    ASSERT_EQ(out.stratum, 2);
    ASSERT_EQ(out.poll, 6);
    ASSERT_EQ(out.precision, -20);
    ASSERT_EQ(out.root_delay, 0x00010000);
    ASSERT_EQ(out.root_dispersion, 0x00008000);
    ASSERT_EQ(out.ref_id, 0x47505300);
    ASSERT_EQ(out.ref_ts.seconds, 3900000000U);
    ASSERT_EQ(out.origin_ts.seconds, 3900000001U);
    ASSERT_EQ(out.recv_ts.seconds, 3900000002U);
    ASSERT_EQ(out.xmit_ts.seconds, 3900000003U);
}

/* ---- Wire format verification ---- */

static void test_wire_byte0(void) {
    struct nfs_ntp_packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.li = 0;
    pkt.vn = 4;
    pkt.mode = 3;
    /* byte0 = (0 << 6) | (4 << 3) | 3 = 0x23 */

    uint8_t buf[48];
    nfs_ntp_build(buf, sizeof(buf), &pkt);
    ASSERT_EQ(buf[0], 0x23);
}

static void test_wire_byte0_li3(void) {
    struct nfs_ntp_packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.li = 3;
    pkt.vn = 4;
    pkt.mode = 4;
    /* byte0 = (3 << 6) | (4 << 3) | 4 = 0xE4 */

    uint8_t buf[48];
    nfs_ntp_build(buf, sizeof(buf), &pkt);
    ASSERT_EQ(buf[0], 0xE4);
}

/* ---- Error cases ---- */

static void test_parse_truncated(void) {
    uint8_t buf[10] = {0};
    struct nfs_ntp_packet out;
    ASSERT_EQ(nfs_ntp_parse(buf, 10, &out), -1);
}

static void test_parse_null(void) {
    struct nfs_ntp_packet out;
    ASSERT_EQ(nfs_ntp_parse(NULL, 48, &out), -1);
}

static void test_build_null(void) {
    ASSERT_EQ(nfs_ntp_build(NULL, 48, NULL), -1);
}

static void test_build_buffer_too_small(void) {
    struct nfs_ntp_packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    uint8_t buf[10];
    ASSERT_EQ(nfs_ntp_build(buf, sizeof(buf), &pkt), -1);
}

/* ---- Timestamp conversion ---- */

static void test_ntp_to_unix_epoch(void) {
    /* NTP seconds = NFS_NTP_UNIX_OFFSET corresponds to Unix epoch 0 */
    struct nfs_ntp_ts ts = {(uint32_t)NFS_NTP_UNIX_OFFSET, 0};
    ASSERT_NEAR(nfs_ntp_to_unix(&ts), 0.0, 0.001);
}

static void test_unix_to_ntp_epoch(void) {
    struct nfs_ntp_ts ts = nfs_unix_to_ntp(0.0);
    ASSERT_EQ(ts.seconds, (uint32_t)NFS_NTP_UNIX_OFFSET);
    ASSERT_EQ(ts.fraction, 0);
}

static void test_timestamp_roundtrip(void) {
    double unix_time = 1700000000.5;
    struct nfs_ntp_ts ts = nfs_unix_to_ntp(unix_time);
    double back = nfs_ntp_to_unix(&ts);
    ASSERT_NEAR(back, unix_time, 0.001);
}

static void test_ntp_to_unix_known(void) {
    /* Unix time 1000000000 = NTP 3208988800 */
    struct nfs_ntp_ts ts = {3208988800U, 0};
    ASSERT_NEAR(nfs_ntp_to_unix(&ts), 1000000000.0, 0.001);
}

static void test_fraction_half(void) {
    /* Fraction 0x80000000 = 0.5 seconds */
    struct nfs_ntp_ts ts = {(uint32_t)NFS_NTP_UNIX_OFFSET + 100, 0x80000000U};
    ASSERT_NEAR(nfs_ntp_to_unix(&ts), 100.5, 0.001);
}

/* ---- Offset and RTT ---- */

static void test_zero_offset_zero_rtt(void) {
    /* All same time -> offset=0, rtt=0 */
    struct nfs_ntp_ts t = {3900000000U, 0};
    ASSERT_NEAR(nfs_ntp_offset(&t, &t, &t, &t), 0.0, 0.001);
    ASSERT_NEAR(nfs_ntp_rtt(&t, &t, &t, &t), 0.0, 0.001);
}

static void test_symmetric_delay(void) {
    /* t1=1000, t2=1000.025, t3=1000.026, t4=1000.050
     * offset = ((0.025) + (0.026 - 0.050)) / 2 = (0.025 - 0.024)/2 = 0.0005
     * rtt = (0.050) - (0.001) = 0.049 */
    struct nfs_ntp_ts t1 = nfs_unix_to_ntp(1000.0);
    struct nfs_ntp_ts t2 = nfs_unix_to_ntp(1000.025);
    struct nfs_ntp_ts t3 = nfs_unix_to_ntp(1000.026);
    struct nfs_ntp_ts t4 = nfs_unix_to_ntp(1000.050);

    double offset = nfs_ntp_offset(&t1, &t2, &t3, &t4);
    double rtt = nfs_ntp_rtt(&t1, &t2, &t3, &t4);
    ASSERT_NEAR(offset, 0.0005, 0.001);
    ASSERT_NEAR(rtt, 0.049, 0.001);
}

static void test_positive_offset(void) {
    /* Server clock is 1 second ahead:
     * t1=100, t2=101.01, t3=101.02, t4=100.02
     * offset = ((1.01) + (1.02 - 0.02)) / 2 = (1.01 + 1.0)/2 = 1.005 */
    struct nfs_ntp_ts t1 = nfs_unix_to_ntp(100.0);
    struct nfs_ntp_ts t2 = nfs_unix_to_ntp(101.01);
    struct nfs_ntp_ts t3 = nfs_unix_to_ntp(101.02);
    struct nfs_ntp_ts t4 = nfs_unix_to_ntp(100.02);

    double offset = nfs_ntp_offset(&t1, &t2, &t3, &t4);
    ASSERT_NEAR(offset, 1.005, 0.002);
}

int main(void) {
    printf("NTP / SNTP Client Tests\n");

    test_client_request_roundtrip();
    test_full_packet_roundtrip();
    test_wire_byte0();
    test_wire_byte0_li3();
    test_parse_truncated();
    test_parse_null();
    test_build_null();
    test_build_buffer_too_small();
    test_ntp_to_unix_epoch();
    test_unix_to_ntp_epoch();
    test_timestamp_roundtrip();
    test_ntp_to_unix_known();
    test_fraction_half();
    test_zero_offset_zero_rtt();
    test_symmetric_delay();
    test_positive_offset();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
