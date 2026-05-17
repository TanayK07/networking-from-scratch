/* Tests for TCP header implementation (RFC 9293).
 *
 * Test families:
 *   1.  Struct size pinning (20 bytes)
 *   2.  Parse SYN packet
 *   3.  Parse SYN-ACK packet
 *   4.  Data offset extraction
 *   5.  Flag extraction for all 8 flags
 *   6.  has_flag checks
 *   7.  Build + parse roundtrip
 *   8.  Build with various flags
 *   9.  Format output
 *  10.  Flag string formatting
 *  11.  Reject truncated buffer
 *  12.  Reject invalid data offset
 *  13.  Parse with options (data offset > 5)
 */

#include "../tcp_hdr.h"

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

/* ---------------------------------------------------------------
 * Test 1: struct size
 * --------------------------------------------------------------- */
static void test_struct_size(void) {
    printf("  struct size...");
    ASSERT_EQ(sizeof(struct nfs_tcp_hdr), 20);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 2: parse SYN packet
 *
 * Hand-crafted TCP SYN:
 *   src_port = 12345 (0x3039)
 *   dst_port = 80    (0x0050)
 *   seq      = 0x00000001
 *   ack      = 0x00000000
 *   doff=5, flags=SYN(0x002) -> data_off_flags = 0x5002
 *   window   = 65535 (0xFFFF)
 *   checksum = 0x0000
 *   urgent   = 0x0000
 * --------------------------------------------------------------- */
static void test_parse_syn(void) {
    printf("  parse SYN...");

    uint8_t raw[] = {
        0x30, 0x39,             /* src_port = 12345 */
        0x00, 0x50,             /* dst_port = 80    */
        0x00, 0x00, 0x00, 0x01, /* seq = 1          */
        0x00, 0x00, 0x00, 0x00, /* ack = 0          */
        0x50, 0x02,             /* doff=5, SYN      */
        0xFF, 0xFF,             /* window = 65535   */
        0x00, 0x00,             /* checksum = 0     */
        0x00, 0x00              /* urgent = 0       */
    };

    struct nfs_tcp_hdr hdr;
    ASSERT_EQ(nfs_tcp_parse(raw, sizeof(raw), &hdr), 0);
    ASSERT_EQ(hdr.src_port, 12345);
    ASSERT_EQ(hdr.dst_port, 80);
    ASSERT_EQ(hdr.seq, 1);
    ASSERT_EQ(hdr.ack, 0);
    ASSERT_EQ(nfs_tcp_data_offset(&hdr), 5);
    ASSERT_EQ(nfs_tcp_flags(&hdr), NFS_TCP_SYN);
    ASSERT_EQ(hdr.window, 65535);
    ASSERT_EQ(hdr.checksum, 0);
    ASSERT_EQ(hdr.urgent, 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: parse SYN-ACK packet
 * --------------------------------------------------------------- */
static void test_parse_synack(void) {
    printf("  parse SYN-ACK...");

    uint8_t raw[] = {
        0x00, 0x50,             /* src_port = 80    */
        0x30, 0x39,             /* dst_port = 12345 */
        0x00, 0x00, 0x10, 0x00, /* seq = 4096       */
        0x00, 0x00, 0x00, 0x02, /* ack = 2          */
        0x50, 0x12,             /* doff=5, SYN+ACK  */
        0x80, 0x00,             /* window = 32768   */
        0xAB, 0xCD,             /* checksum         */
        0x00, 0x00              /* urgent = 0       */
    };

    struct nfs_tcp_hdr hdr;
    ASSERT_EQ(nfs_tcp_parse(raw, sizeof(raw), &hdr), 0);
    ASSERT_EQ(hdr.src_port, 80);
    ASSERT_EQ(hdr.dst_port, 12345);
    ASSERT_EQ(hdr.seq, 4096);
    ASSERT_EQ(hdr.ack, 2);
    ASSERT_EQ(nfs_tcp_flags(&hdr), NFS_TCP_SYN | NFS_TCP_ACK);
    ASSERT_EQ(hdr.window, 32768);
    ASSERT_EQ(hdr.checksum, 0xABCD);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: data offset extraction
 * --------------------------------------------------------------- */
static void test_data_offset(void) {
    printf("  data offset extraction...");

    /* data_off_flags with doff=5 (0x5xxx) */
    struct nfs_tcp_hdr h;
    memset(&h, 0, sizeof(h));
    h.data_off_flags = 0x5002; /* doff=5, SYN */
    ASSERT_EQ(nfs_tcp_data_offset(&h), 5);

    /* doff=6 (24 bytes, with options) */
    h.data_off_flags = 0x6002;
    ASSERT_EQ(nfs_tcp_data_offset(&h), 6);

    /* doff=15 (maximum, 60 bytes) */
    h.data_off_flags = 0xF002;
    ASSERT_EQ(nfs_tcp_data_offset(&h), 15);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: flag extraction for all 8 flags
 * --------------------------------------------------------------- */
static void test_flag_extraction(void) {
    printf("  flag extraction...");

    struct nfs_tcp_hdr h;
    memset(&h, 0, sizeof(h));

    /* Test each flag individually. */
    h.data_off_flags = 0x5000 | NFS_TCP_FIN;
    ASSERT_EQ(nfs_tcp_flags(&h), NFS_TCP_FIN);

    h.data_off_flags = 0x5000 | NFS_TCP_SYN;
    ASSERT_EQ(nfs_tcp_flags(&h), NFS_TCP_SYN);

    h.data_off_flags = 0x5000 | NFS_TCP_RST;
    ASSERT_EQ(nfs_tcp_flags(&h), NFS_TCP_RST);

    h.data_off_flags = 0x5000 | NFS_TCP_PSH;
    ASSERT_EQ(nfs_tcp_flags(&h), NFS_TCP_PSH);

    h.data_off_flags = 0x5000 | NFS_TCP_ACK;
    ASSERT_EQ(nfs_tcp_flags(&h), NFS_TCP_ACK);

    h.data_off_flags = 0x5000 | NFS_TCP_URG;
    ASSERT_EQ(nfs_tcp_flags(&h), NFS_TCP_URG);

    h.data_off_flags = 0x5000 | NFS_TCP_ECE;
    ASSERT_EQ(nfs_tcp_flags(&h), NFS_TCP_ECE);

    h.data_off_flags = 0x5000 | NFS_TCP_CWR;
    ASSERT_EQ(nfs_tcp_flags(&h), NFS_TCP_CWR);

    /* All flags set. */
    h.data_off_flags = 0x5000 | 0x0FF;
    ASSERT_EQ(nfs_tcp_flags(&h), 0x0FF);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: has_flag checks
 * --------------------------------------------------------------- */
static void test_has_flag(void) {
    printf("  has_flag...");

    struct nfs_tcp_hdr h;
    memset(&h, 0, sizeof(h));
    h.data_off_flags = 0x5000 | NFS_TCP_SYN | NFS_TCP_ACK;

    ASSERT_EQ(nfs_tcp_has_flag(&h, NFS_TCP_SYN), 1);
    ASSERT_EQ(nfs_tcp_has_flag(&h, NFS_TCP_ACK), 1);
    ASSERT_EQ(nfs_tcp_has_flag(&h, NFS_TCP_FIN), 0);
    ASSERT_EQ(nfs_tcp_has_flag(&h, NFS_TCP_RST), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: build + parse roundtrip
 * --------------------------------------------------------------- */
static void test_build_parse_roundtrip(void) {
    printf("  build+parse roundtrip...");

    uint8_t buf[64];
    int n =
        nfs_tcp_build(8080, 443, 1000, 2000, NFS_TCP_SYN | NFS_TCP_ACK, 32768, buf, sizeof(buf));
    ASSERT_EQ(n, 20);

    struct nfs_tcp_hdr hdr;
    ASSERT_EQ(nfs_tcp_parse(buf, (size_t)n, &hdr), 0);
    ASSERT_EQ(hdr.src_port, 8080);
    ASSERT_EQ(hdr.dst_port, 443);
    ASSERT_EQ(hdr.seq, 1000);
    ASSERT_EQ(hdr.ack, 2000);
    ASSERT_EQ(nfs_tcp_data_offset(&hdr), 5);
    ASSERT_EQ(nfs_tcp_flags(&hdr), NFS_TCP_SYN | NFS_TCP_ACK);
    ASSERT_EQ(hdr.window, 32768);
    ASSERT_EQ(hdr.checksum, 0);
    ASSERT_EQ(hdr.urgent, 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: build with various flags
 * --------------------------------------------------------------- */
static void test_build_various_flags(void) {
    printf("  build various flags...");

    uint8_t buf[64];
    struct nfs_tcp_hdr hdr;

    /* FIN+ACK */
    int n = nfs_tcp_build(80, 12345, 500, 600, NFS_TCP_FIN | NFS_TCP_ACK, 1024, buf, sizeof(buf));
    ASSERT_EQ(n, 20);
    ASSERT_EQ(nfs_tcp_parse(buf, (size_t)n, &hdr), 0);
    ASSERT_EQ(nfs_tcp_flags(&hdr), NFS_TCP_FIN | NFS_TCP_ACK);

    /* RST */
    n = nfs_tcp_build(80, 12345, 0, 0, NFS_TCP_RST, 0, buf, sizeof(buf));
    ASSERT_EQ(n, 20);
    ASSERT_EQ(nfs_tcp_parse(buf, (size_t)n, &hdr), 0);
    ASSERT_EQ(nfs_tcp_flags(&hdr), NFS_TCP_RST);

    /* PSH+ACK */
    n = nfs_tcp_build(443, 8080, 100, 200, NFS_TCP_PSH | NFS_TCP_ACK, 65535, buf, sizeof(buf));
    ASSERT_EQ(n, 20);
    ASSERT_EQ(nfs_tcp_parse(buf, (size_t)n, &hdr), 0);
    ASSERT_EQ(nfs_tcp_flags(&hdr), NFS_TCP_PSH | NFS_TCP_ACK);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: format output
 * --------------------------------------------------------------- */
static void test_format_output(void) {
    printf("  format output...");

    struct nfs_tcp_hdr h;
    memset(&h, 0, sizeof(h));
    h.src_port = 12345;
    h.dst_port = 80;
    h.seq = 1000;
    h.ack = 2000;
    h.data_off_flags = 0x5000 | NFS_TCP_SYN | NFS_TCP_ACK;
    h.window = 65535;

    char buf[256];
    nfs_tcp_format(&h, buf, sizeof(buf));

    ASSERT_TRUE(strstr(buf, "12345") != NULL);
    ASSERT_TRUE(strstr(buf, "80") != NULL);
    ASSERT_TRUE(strstr(buf, "1000") != NULL);
    ASSERT_TRUE(strstr(buf, "2000") != NULL);
    ASSERT_TRUE(strstr(buf, "SYN") != NULL);
    ASSERT_TRUE(strstr(buf, "ACK") != NULL);
    ASSERT_TRUE(strstr(buf, "65535") != NULL);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 10: flag string formatting
 * --------------------------------------------------------------- */
static void test_flag_str(void) {
    printf("  flag string...");

    const char *s;

    s = nfs_tcp_flag_str(NFS_TCP_SYN);
    ASSERT_TRUE(strstr(s, "SYN") != NULL);

    s = nfs_tcp_flag_str(NFS_TCP_SYN | NFS_TCP_ACK);
    ASSERT_TRUE(strstr(s, "SYN") != NULL);
    ASSERT_TRUE(strstr(s, "ACK") != NULL);

    s = nfs_tcp_flag_str(NFS_TCP_FIN | NFS_TCP_ACK);
    ASSERT_TRUE(strstr(s, "FIN") != NULL);
    ASSERT_TRUE(strstr(s, "ACK") != NULL);

    s = nfs_tcp_flag_str(NFS_TCP_RST);
    ASSERT_TRUE(strstr(s, "RST") != NULL);

    s = nfs_tcp_flag_str(0);
    ASSERT_EQ(strlen(s), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 11: reject truncated buffer
 * --------------------------------------------------------------- */
static void test_reject_truncated(void) {
    printf("  reject truncated...");

    struct nfs_tcp_hdr hdr;

    ASSERT_EQ(nfs_tcp_parse(NULL, 0, &hdr), -1);

    uint8_t short_buf[10] = {0};
    ASSERT_EQ(nfs_tcp_parse(short_buf, sizeof(short_buf), &hdr), -1);

    uint8_t almost[19] = {0};
    ASSERT_EQ(nfs_tcp_parse(almost, sizeof(almost), &hdr), -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 12: reject invalid data offset (< 5)
 * --------------------------------------------------------------- */
static void test_reject_invalid_doff(void) {
    printf("  reject invalid data offset...");

    /* data_off = 4 (0x4002) -- invalid, minimum is 5. */
    uint8_t raw[20] = {0};
    raw[12] = 0x40; /* doff = 4 */
    raw[13] = 0x02; /* SYN */

    struct nfs_tcp_hdr hdr;
    ASSERT_EQ(nfs_tcp_parse(raw, sizeof(raw), &hdr), -1);

    /* data_off = 0 */
    raw[12] = 0x00;
    ASSERT_EQ(nfs_tcp_parse(raw, sizeof(raw), &hdr), -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 13: parse with options (data offset > 5)
 *
 * data_off = 6 means 24 bytes of header (4 bytes of options).
 * We need to supply at least 24 bytes.
 * --------------------------------------------------------------- */
static void test_parse_with_options(void) {
    printf("  parse with options...");

    uint8_t raw[24] = {
        0x00, 0x50,             /* src_port = 80    */
        0x30, 0x39,             /* dst_port = 12345 */
        0x00, 0x00, 0x00, 0x01, /* seq = 1          */
        0x00, 0x00, 0x00, 0x00, /* ack = 0          */
        0x60, 0x02,             /* doff=6, SYN      */
        0xFF, 0xFF,             /* window = 65535   */
        0x00, 0x00,             /* checksum = 0     */
        0x00, 0x00,             /* urgent = 0       */
        0x02, 0x04, 0x05, 0xB4  /* MSS option: 1460 */
    };

    struct nfs_tcp_hdr hdr;
    ASSERT_EQ(nfs_tcp_parse(raw, sizeof(raw), &hdr), 0);
    ASSERT_EQ(nfs_tcp_data_offset(&hdr), 6);
    ASSERT_EQ(hdr.src_port, 80);
    ASSERT_EQ(nfs_tcp_flags(&hdr), NFS_TCP_SYN);

    /* Fail if buffer too short for declared data offset. */
    ASSERT_EQ(nfs_tcp_parse(raw, 20, &hdr), -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */
int main(void) {
    printf("TCP header (RFC 9293) test suite\n");

    test_struct_size();
    test_parse_syn();
    test_parse_synack();
    test_data_offset();
    test_flag_extraction();
    test_has_flag();
    test_build_parse_roundtrip();
    test_build_various_flags();
    test_format_output();
    test_flag_str();
    test_reject_truncated();
    test_reject_invalid_doff();
    test_parse_with_options();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
