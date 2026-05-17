/* Tests for TCP options encoder/decoder.
 *
 * Test families:
 *   1.  Build+parse MSS roundtrip
 *   2.  Build+parse WScale roundtrip
 *   3.  Build+parse Timestamps roundtrip
 *   4.  SACK Permitted
 *   5.  NOP padding to 4-byte boundary
 *   6.  Multiple options in sequence
 *   7.  EOL terminates parsing
 *   8.  Malformed option (length=0) rejected
 *   9.  Truncated option rejected
 *  10.  MSS value edge cases
 *  11.  Format strings
 *  12.  Insufficient buffer rejected
 */

#include "tcp_opts.h"

#include <stdio.h>
#include <stdlib.h>
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
 * Test 1: Build+parse MSS roundtrip
 * --------------------------------------------------------------- */

static void test_mss_roundtrip(void) {
    printf("  MSS roundtrip...");

    uint8_t buf[16];
    int n = nfs_tcp_opts_build_mss(1460, buf, sizeof(buf));
    ASSERT_EQ(n, 4);
    ASSERT_EQ(buf[0], NFS_TCPOPT_MSS);
    ASSERT_EQ(buf[1], 4);

    struct nfs_tcp_option opts[4];
    size_t nfound;
    int rc = nfs_tcp_opts_parse(buf, (size_t)n, opts, 4, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(nfound, 1);
    ASSERT_EQ(opts[0].kind, NFS_TCPOPT_MSS);
    ASSERT_EQ(opts[0].length, 4);
    ASSERT_EQ(opts[0].data.mss, 1460);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 2: Build+parse WScale roundtrip
 * --------------------------------------------------------------- */

static void test_wscale_roundtrip(void) {
    printf("  WScale roundtrip...");

    uint8_t buf[16];
    int n = nfs_tcp_opts_build_wscale(7, buf, sizeof(buf));
    ASSERT_EQ(n, 3);
    ASSERT_EQ(buf[0], NFS_TCPOPT_WSCALE);
    ASSERT_EQ(buf[1], 3);
    ASSERT_EQ(buf[2], 7);

    struct nfs_tcp_option opts[4];
    size_t nfound;
    int rc = nfs_tcp_opts_parse(buf, (size_t)n, opts, 4, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(nfound, 1);
    ASSERT_EQ(opts[0].kind, NFS_TCPOPT_WSCALE);
    ASSERT_EQ(opts[0].data.wscale, 7);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: Build+parse Timestamps roundtrip
 * --------------------------------------------------------------- */

static void test_timestamps_roundtrip(void) {
    printf("  Timestamps roundtrip...");

    uint8_t buf[16];
    int n = nfs_tcp_opts_build_timestamps(123456, 789012, buf, sizeof(buf));
    ASSERT_EQ(n, 10);
    ASSERT_EQ(buf[0], NFS_TCPOPT_TIMESTAMPS);
    ASSERT_EQ(buf[1], 10);

    struct nfs_tcp_option opts[4];
    size_t nfound;
    int rc = nfs_tcp_opts_parse(buf, (size_t)n, opts, 4, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(nfound, 1);
    ASSERT_EQ(opts[0].kind, NFS_TCPOPT_TIMESTAMPS);
    ASSERT_EQ(opts[0].data.timestamps.ts_val, 123456);
    ASSERT_EQ(opts[0].data.timestamps.ts_ecr, 789012);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: SACK Permitted
 * --------------------------------------------------------------- */

static void test_sack_permitted(void) {
    printf("  SACK Permitted...");

    uint8_t buf[8];
    int n = nfs_tcp_opts_build_sack_perm(buf, sizeof(buf));
    ASSERT_EQ(n, 2);
    ASSERT_EQ(buf[0], NFS_TCPOPT_SACK_PERM);
    ASSERT_EQ(buf[1], 2);

    struct nfs_tcp_option opts[4];
    size_t nfound;
    int rc = nfs_tcp_opts_parse(buf, (size_t)n, opts, 4, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(nfound, 1);
    ASSERT_EQ(opts[0].kind, NFS_TCPOPT_SACK_PERM);
    ASSERT_EQ(opts[0].length, 2);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: NOP padding to 4-byte boundary
 * --------------------------------------------------------------- */

static void test_padding(void) {
    printf("  NOP padding to 4-byte boundary...");

    uint8_t buf[32];

    /* WScale is 3 bytes — needs 1 byte of padding to reach 4 */
    int n = nfs_tcp_opts_build_wscale(5, buf, sizeof(buf));
    ASSERT_EQ(n, 3);

    size_t padded;
    int rc = nfs_tcp_opts_pad(buf, (size_t)n, &padded);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(padded, 4); /* 3 rounded up to 4 */

    /* MSS is 4 bytes — already aligned */
    n = nfs_tcp_opts_build_mss(1460, buf, sizeof(buf));
    rc = nfs_tcp_opts_pad(buf, (size_t)n, &padded);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(padded, 4); /* already aligned */

    /* SACK-Perm is 2 bytes — needs 2 bytes padding to reach 4 */
    n = nfs_tcp_opts_build_sack_perm(buf, sizeof(buf));
    rc = nfs_tcp_opts_pad(buf, (size_t)n, &padded);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(padded, 4);

    /* Timestamps is 10 bytes — needs 2 bytes padding to reach 12 */
    n = nfs_tcp_opts_build_timestamps(1, 2, buf, sizeof(buf));
    rc = nfs_tcp_opts_pad(buf, (size_t)n, &padded);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(padded, 12);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: Multiple options in sequence
 * --------------------------------------------------------------- */

static void test_multiple_options(void) {
    printf("  multiple options...");

    uint8_t buf[64];
    size_t pos = 0;
    int n;

    n = nfs_tcp_opts_build_mss(1460, buf + pos, sizeof(buf) - pos);
    ASSERT_TRUE(n > 0);
    pos += (size_t)n;

    n = nfs_tcp_opts_build_wscale(7, buf + pos, sizeof(buf) - pos);
    ASSERT_TRUE(n > 0);
    pos += (size_t)n;

    n = nfs_tcp_opts_build_sack_perm(buf + pos, sizeof(buf) - pos);
    ASSERT_TRUE(n > 0);
    pos += (size_t)n;

    n = nfs_tcp_opts_build_timestamps(100, 200, buf + pos, sizeof(buf) - pos);
    ASSERT_TRUE(n > 0);
    pos += (size_t)n;

    /* Parse all of them */
    struct nfs_tcp_option opts[8];
    size_t nfound;
    int rc = nfs_tcp_opts_parse(buf, pos, opts, 8, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(nfound, 4);

    ASSERT_EQ(opts[0].kind, NFS_TCPOPT_MSS);
    ASSERT_EQ(opts[0].data.mss, 1460);

    ASSERT_EQ(opts[1].kind, NFS_TCPOPT_WSCALE);
    ASSERT_EQ(opts[1].data.wscale, 7);

    ASSERT_EQ(opts[2].kind, NFS_TCPOPT_SACK_PERM);

    ASSERT_EQ(opts[3].kind, NFS_TCPOPT_TIMESTAMPS);
    ASSERT_EQ(opts[3].data.timestamps.ts_val, 100);
    ASSERT_EQ(opts[3].data.timestamps.ts_ecr, 200);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: EOL terminates parsing
 * --------------------------------------------------------------- */

static void test_eol_terminates(void) {
    printf("  EOL terminates parsing...");

    uint8_t buf[16];
    size_t pos = 0;
    int n;

    /* Build MSS */
    n = nfs_tcp_opts_build_mss(536, buf + pos, sizeof(buf) - pos);
    pos += (size_t)n;

    /* Insert EOL */
    buf[pos++] = NFS_TCPOPT_EOL;

    /* Put WScale after EOL — should NOT be parsed */
    n = nfs_tcp_opts_build_wscale(3, buf + pos, sizeof(buf) - pos);
    pos += (size_t)n;

    struct nfs_tcp_option opts[8];
    size_t nfound;
    int rc = nfs_tcp_opts_parse(buf, pos, opts, 8, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(nfound, 1); /* Only MSS, EOL stops parsing */
    ASSERT_EQ(opts[0].kind, NFS_TCPOPT_MSS);
    ASSERT_EQ(opts[0].data.mss, 536);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: Malformed option (length=0) rejected
 * --------------------------------------------------------------- */

static void test_malformed_length_zero(void) {
    printf("  malformed option (length=0) rejected...");

    uint8_t buf[] = {NFS_TCPOPT_MSS, 0, 0, 0};
    struct nfs_tcp_option opts[4];
    size_t nfound;
    int rc = nfs_tcp_opts_parse(buf, sizeof(buf), opts, 4, &nfound);
    ASSERT_EQ(rc, -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: Truncated option rejected
 * --------------------------------------------------------------- */

static void test_truncated(void) {
    printf("  truncated option rejected...");

    /* MSS claims length=4 but only 3 bytes present */
    uint8_t buf[] = {NFS_TCPOPT_MSS, 4, 0x05};
    struct nfs_tcp_option opts[4];
    size_t nfound;
    int rc = nfs_tcp_opts_parse(buf, sizeof(buf), opts, 4, &nfound);
    ASSERT_EQ(rc, -1);

    /* Only kind byte, no length */
    uint8_t buf2[] = {NFS_TCPOPT_MSS};
    rc = nfs_tcp_opts_parse(buf2, sizeof(buf2), opts, 4, &nfound);
    ASSERT_EQ(rc, -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 10: MSS value edge cases
 * --------------------------------------------------------------- */

static void test_mss_edge_cases(void) {
    printf("  MSS edge cases...");

    uint8_t buf[8];
    struct nfs_tcp_option opts[4];
    size_t nfound;
    int n, rc;

    /* Minimum MSS */
    n = nfs_tcp_opts_build_mss(1, buf, sizeof(buf));
    ASSERT_EQ(n, 4);
    rc = nfs_tcp_opts_parse(buf, (size_t)n, opts, 4, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(opts[0].data.mss, 1);

    /* Maximum MSS */
    n = nfs_tcp_opts_build_mss(65535, buf, sizeof(buf));
    ASSERT_EQ(n, 4);
    rc = nfs_tcp_opts_parse(buf, (size_t)n, opts, 4, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(opts[0].data.mss, 65535);

    /* Common Ethernet MSS */
    n = nfs_tcp_opts_build_mss(1460, buf, sizeof(buf));
    rc = nfs_tcp_opts_parse(buf, (size_t)n, opts, 4, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(opts[0].data.mss, 1460);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 11: Format strings
 * --------------------------------------------------------------- */

static void test_format(void) {
    printf("  format strings...");

    struct nfs_tcp_option opt;
    char buf[128];

    /* MSS */
    opt.kind = NFS_TCPOPT_MSS;
    opt.data.mss = 1460;
    nfs_tcp_opt_format(&opt, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "MSS=1460") != NULL);

    /* WScale */
    opt.kind = NFS_TCPOPT_WSCALE;
    opt.data.wscale = 7;
    nfs_tcp_opt_format(&opt, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "WScale=7") != NULL);

    /* Timestamps */
    opt.kind = NFS_TCPOPT_TIMESTAMPS;
    opt.data.timestamps.ts_val = 123;
    opt.data.timestamps.ts_ecr = 456;
    nfs_tcp_opt_format(&opt, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "TS val=123 ecr=456") != NULL);

    /* NOP */
    opt.kind = NFS_TCPOPT_NOP;
    nfs_tcp_opt_format(&opt, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "NOP") != NULL);

    /* EOL */
    opt.kind = NFS_TCPOPT_EOL;
    nfs_tcp_opt_format(&opt, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "EOL") != NULL);

    /* SACK Permitted */
    opt.kind = NFS_TCPOPT_SACK_PERM;
    nfs_tcp_opt_format(&opt, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "SACK") != NULL);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 12: Insufficient buffer rejected
 * --------------------------------------------------------------- */

static void test_insufficient_buffer(void) {
    printf("  insufficient buffer rejected...");

    uint8_t tiny[1];
    ASSERT_EQ(nfs_tcp_opts_build_mss(1460, tiny, sizeof(tiny)), -1);
    ASSERT_EQ(nfs_tcp_opts_build_wscale(7, tiny, sizeof(tiny)), -1);
    ASSERT_EQ(nfs_tcp_opts_build_timestamps(1, 2, tiny, sizeof(tiny)), -1);
    ASSERT_EQ(nfs_tcp_opts_build_sack_perm(tiny, sizeof(tiny)), -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 13: NOP in option stream parsed correctly
 * --------------------------------------------------------------- */

static void test_nop_in_stream(void) {
    printf("  NOP in option stream...");

    /* NOP, NOP, MSS(1460) — typical alignment pattern */
    uint8_t buf[16];
    buf[0] = NFS_TCPOPT_NOP;
    buf[1] = NFS_TCPOPT_NOP;
    int n = nfs_tcp_opts_build_mss(1460, buf + 2, sizeof(buf) - 2);
    ASSERT_EQ(n, 4);

    struct nfs_tcp_option opts[8];
    size_t nfound;
    int rc = nfs_tcp_opts_parse(buf, 6, opts, 8, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(nfound, 3);
    ASSERT_EQ(opts[0].kind, NFS_TCPOPT_NOP);
    ASSERT_EQ(opts[1].kind, NFS_TCPOPT_NOP);
    ASSERT_EQ(opts[2].kind, NFS_TCPOPT_MSS);
    ASSERT_EQ(opts[2].data.mss, 1460);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 14: WScale=0 and WScale=14 (max)
 * --------------------------------------------------------------- */

static void test_wscale_extremes(void) {
    printf("  WScale extremes...");

    uint8_t buf[8];
    struct nfs_tcp_option opts[4];
    size_t nfound;

    nfs_tcp_opts_build_wscale(0, buf, sizeof(buf));
    nfs_tcp_opts_parse(buf, 3, opts, 4, &nfound);
    ASSERT_EQ(opts[0].data.wscale, 0);

    nfs_tcp_opts_build_wscale(14, buf, sizeof(buf));
    nfs_tcp_opts_parse(buf, 3, opts, 4, &nfound);
    ASSERT_EQ(opts[0].data.wscale, 14);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 15: Wrong length for MSS rejected
 * --------------------------------------------------------------- */

static void test_wrong_mss_length(void) {
    printf("  wrong MSS length rejected...");

    /* MSS with length=5 instead of 4 */
    uint8_t buf[] = {NFS_TCPOPT_MSS, 5, 0x05, 0xB4, 0x00};
    struct nfs_tcp_option opts[4];
    size_t nfound;
    int rc = nfs_tcp_opts_parse(buf, sizeof(buf), opts, 4, &nfound);
    ASSERT_EQ(rc, -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void) {
    printf("TCP options encoder/decoder test suite\n");

    test_mss_roundtrip();
    test_wscale_roundtrip();
    test_timestamps_roundtrip();
    test_sack_permitted();
    test_padding();
    test_multiple_options();
    test_eol_terminates();
    test_malformed_length_zero();
    test_truncated();
    test_mss_edge_cases();
    test_format();
    test_insufficient_buffer();
    test_nop_in_stream();
    test_wscale_extremes();
    test_wrong_mss_length();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
