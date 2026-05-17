/* Unit tests for nfs_ip_opts (IPv4 options parser/builder). */

#include "../ip_opts.h"
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

/* ---- parse EOL only ---- */

static void test_parse_eol_only(void) {
    uint8_t data[] = {NFS_IPOPT_EOL};
    struct nfs_ip_option opts[4];
    size_t nfound;

    ASSERT_EQ(nfs_ip_opts_parse(data, sizeof(data), opts, 4, &nfound), 0);
    ASSERT_EQ(nfound, 1);
    ASSERT_EQ(opts[0].type, NFS_IPOPT_EOL);
}

/* ---- parse NOP stream ---- */

static void test_parse_nop_stream(void) {
    uint8_t data[] = {NFS_IPOPT_NOP, NFS_IPOPT_NOP, NFS_IPOPT_NOP, NFS_IPOPT_EOL};
    struct nfs_ip_option opts[8];
    size_t nfound;

    ASSERT_EQ(nfs_ip_opts_parse(data, sizeof(data), opts, 8, &nfound), 0);
    ASSERT_EQ(nfound, 4);
    ASSERT_EQ(opts[0].type, NFS_IPOPT_NOP);
    ASSERT_EQ(opts[1].type, NFS_IPOPT_NOP);
    ASSERT_EQ(opts[2].type, NFS_IPOPT_NOP);
    ASSERT_EQ(opts[3].type, NFS_IPOPT_EOL);
}

/* ---- parse Record Route ---- */

static void test_parse_record_route(void) {
    /* RR with 2 hops: type=7, length=11, pointer=4, 8 bytes of IPs */
    uint8_t data[] = {
        0x07, 0x0B, 0x04,       /* type, len, ptr */
        0xC0, 0xA8, 0x01, 0x01, /* 192.168.1.1 */
        0x0A, 0x00, 0x00, 0x01, /* 10.0.0.1 */
    };
    struct nfs_ip_option opts[4];
    size_t nfound;

    ASSERT_EQ(nfs_ip_opts_parse(data, sizeof(data), opts, 4, &nfound), 0);
    ASSERT_EQ(nfound, 1);
    ASSERT_EQ(opts[0].type, NFS_IPOPT_RR);
    ASSERT_EQ(opts[0].length, 11);
    /* Pointer should be in data[0] */
    ASSERT_EQ(opts[0].data[0], 0x04);
    /* First IP in data[1..4] */
    ASSERT_EQ(opts[0].data[1], 0xC0);
    ASSERT_EQ(opts[0].data[2], 0xA8);
}

/* ---- build + parse RR roundtrip ---- */

static void test_build_parse_rr_roundtrip(void) {
    uint8_t buf[40];
    int n = nfs_ip_opts_build_rr(3, buf, sizeof(buf));
    /* 3 hops: 3 + 3*4 = 15 bytes */
    ASSERT_EQ(n, 15);
    ASSERT_EQ(buf[0], NFS_IPOPT_RR);
    ASSERT_EQ(buf[1], 15);
    ASSERT_EQ(buf[2], 4); /* pointer */

    /* Parse it back */
    struct nfs_ip_option opts[4];
    size_t nfound;
    ASSERT_EQ(nfs_ip_opts_parse(buf, (size_t)n, opts, 4, &nfound), 0);
    ASSERT_EQ(nfound, 1);
    ASSERT_EQ(opts[0].type, NFS_IPOPT_RR);
    ASSERT_EQ(opts[0].length, 15);
}

/* ---- build + parse TS roundtrip ---- */

static void test_build_parse_ts_roundtrip(void) {
    uint8_t buf[40];
    int n = nfs_ip_opts_build_ts(2, buf, sizeof(buf));
    /* 2 entries: 4 + 2*4 = 12 bytes */
    ASSERT_EQ(n, 12);
    ASSERT_EQ(buf[0], NFS_IPOPT_TS);
    ASSERT_EQ(buf[1], 12);
    ASSERT_EQ(buf[2], 5); /* pointer */
    ASSERT_EQ(buf[3], 0); /* oflw/flag */

    /* Parse it back */
    struct nfs_ip_option opts[4];
    size_t nfound;
    ASSERT_EQ(nfs_ip_opts_parse(buf, (size_t)n, opts, 4, &nfound), 0);
    ASSERT_EQ(nfound, 1);
    ASSERT_EQ(opts[0].type, NFS_IPOPT_TS);
    ASSERT_EQ(opts[0].length, 12);
}

/* ---- pad to 4-byte boundary ---- */

static void test_pad_alignment(void) {
    /* 3 bytes of NOP -> pad to 4 */
    uint8_t buf[40] = {NFS_IPOPT_NOP, NFS_IPOPT_NOP, NFS_IPOPT_NOP};
    size_t padded;

    ASSERT_EQ(nfs_ip_opts_pad(buf, 3, &padded), 0);
    ASSERT_EQ(padded, 4);
    ASSERT_EQ(buf[3], NFS_IPOPT_EOL);
}

static void test_pad_already_aligned(void) {
    uint8_t buf[40] = {NFS_IPOPT_NOP, NFS_IPOPT_NOP, NFS_IPOPT_NOP, NFS_IPOPT_NOP};
    size_t padded;

    ASSERT_EQ(nfs_ip_opts_pad(buf, 4, &padded), 0);
    ASSERT_EQ(padded, 4);
}

static void test_pad_5_bytes(void) {
    /* 5 bytes -> pad to 8 */
    uint8_t buf[40];
    memset(buf, NFS_IPOPT_NOP, 5);
    size_t padded;

    ASSERT_EQ(nfs_ip_opts_pad(buf, 5, &padded), 0);
    ASSERT_EQ(padded, 8);
    /* bytes 5,6 should be NOP, byte 7 should be EOL */
    ASSERT_EQ(buf[5], NFS_IPOPT_NOP);
    ASSERT_EQ(buf[6], NFS_IPOPT_NOP);
    ASSERT_EQ(buf[7], NFS_IPOPT_EOL);
}

/* ---- malformed: length = 0 ---- */

static void test_malformed_zero_length(void) {
    /* A multi-byte option with length 0 is invalid */
    uint8_t data[] = {NFS_IPOPT_RR, 0x00};
    struct nfs_ip_option opts[4];
    size_t nfound;

    ASSERT_EQ(nfs_ip_opts_parse(data, sizeof(data), opts, 4, &nfound), -1);
}

/* ---- malformed: length = 1 ---- */

static void test_malformed_length_one(void) {
    uint8_t data[] = {NFS_IPOPT_RR, 0x01};
    struct nfs_ip_option opts[4];
    size_t nfound;

    ASSERT_EQ(nfs_ip_opts_parse(data, sizeof(data), opts, 4, &nfound), -1);
}

/* ---- malformed: length exceeds buffer ---- */

static void test_malformed_length_exceeds(void) {
    uint8_t data[] = {NFS_IPOPT_RR, 0x10, 0x04}; /* claims 16 bytes but only 3 */
    struct nfs_ip_option opts[4];
    size_t nfound;

    ASSERT_EQ(nfs_ip_opts_parse(data, sizeof(data), opts, 4, &nfound), -1);
}

/* ---- malformed: truncated (no length byte) ---- */

static void test_malformed_truncated(void) {
    uint8_t data[] = {NFS_IPOPT_RR}; /* only type, no length */
    struct nfs_ip_option opts[4];
    size_t nfound;

    ASSERT_EQ(nfs_ip_opts_parse(data, sizeof(data), opts, 4, &nfound), -1);
}

/* ---- option type names ---- */

static void test_type_names(void) {
    ASSERT_TRUE(strcmp(nfs_ip_opt_name(NFS_IPOPT_EOL), "EOL") == 0);
    ASSERT_TRUE(strcmp(nfs_ip_opt_name(NFS_IPOPT_NOP), "NOP") == 0);
    ASSERT_TRUE(strcmp(nfs_ip_opt_name(NFS_IPOPT_RR), "Record Route") == 0);
    ASSERT_TRUE(strcmp(nfs_ip_opt_name(NFS_IPOPT_TS), "Timestamp") == 0);
    ASSERT_TRUE(strcmp(nfs_ip_opt_name(NFS_IPOPT_LSRR), "Loose Source Route") == 0);
    ASSERT_TRUE(strcmp(nfs_ip_opt_name(NFS_IPOPT_SSRR), "Strict Source Route") == 0);
    ASSERT_TRUE(strcmp(nfs_ip_opt_name(255), "Unknown") == 0);
}

/* ---- build RR invalid args ---- */

static void test_build_rr_invalid(void) {
    uint8_t buf[40];
    ASSERT_EQ(nfs_ip_opts_build_rr(0, buf, sizeof(buf)), -1);
    ASSERT_EQ(nfs_ip_opts_build_rr(1, NULL, 40), -1);
    /* Too many hops: 3 + 10*4 = 43 > 40 */
    ASSERT_EQ(nfs_ip_opts_build_rr(10, buf, sizeof(buf)), -1);
}

/* ---- build TS invalid args ---- */

static void test_build_ts_invalid(void) {
    uint8_t buf[40];
    ASSERT_EQ(nfs_ip_opts_build_ts(0, buf, sizeof(buf)), -1);
    ASSERT_EQ(nfs_ip_opts_build_ts(1, NULL, 40), -1);
    /* Too many entries: 4 + 10*4 = 44 > 40 */
    ASSERT_EQ(nfs_ip_opts_build_ts(10, buf, sizeof(buf)), -1);
}

/* ---- parse NULL args ---- */

static void test_parse_null(void) {
    struct nfs_ip_option opts[4];
    size_t nfound;
    ASSERT_EQ(nfs_ip_opts_parse(NULL, 4, opts, 4, &nfound), -1);

    uint8_t data[] = {0};
    ASSERT_EQ(nfs_ip_opts_parse(data, 1, NULL, 4, &nfound), -1);
    ASSERT_EQ(nfs_ip_opts_parse(data, 1, opts, 4, NULL), -1);
}

/* ---- parse multiple options ---- */

static void test_parse_mixed(void) {
    /* NOP + RR(7 bytes) + NOP + EOL */
    uint8_t data[] = {
        NFS_IPOPT_NOP, NFS_IPOPT_RR, 0x07, 0x04,          0xC0,
        0xA8,          0x01,         0x01, NFS_IPOPT_NOP, NFS_IPOPT_EOL,
    };
    struct nfs_ip_option opts[8];
    size_t nfound;

    ASSERT_EQ(nfs_ip_opts_parse(data, sizeof(data), opts, 8, &nfound), 0);
    ASSERT_EQ(nfound, 4);
    ASSERT_EQ(opts[0].type, NFS_IPOPT_NOP);
    ASSERT_EQ(opts[1].type, NFS_IPOPT_RR);
    ASSERT_EQ(opts[1].length, 7);
    ASSERT_EQ(opts[2].type, NFS_IPOPT_NOP);
    ASSERT_EQ(opts[3].type, NFS_IPOPT_EOL);
}

/* ---- build RR single hop ---- */

static void test_build_rr_single(void) {
    uint8_t buf[40];
    int n = nfs_ip_opts_build_rr(1, buf, sizeof(buf));
    ASSERT_EQ(n, 7); /* 3 + 1*4 */
    ASSERT_EQ(buf[0], NFS_IPOPT_RR);
    ASSERT_EQ(buf[1], 7);
    ASSERT_EQ(buf[2], 4);
}

/* ---- build TS single entry ---- */

static void test_build_ts_single(void) {
    uint8_t buf[40];
    int n = nfs_ip_opts_build_ts(1, buf, sizeof(buf));
    ASSERT_EQ(n, 8); /* 4 + 1*4 */
    ASSERT_EQ(buf[0], NFS_IPOPT_TS);
    ASSERT_EQ(buf[1], 8);
    ASSERT_EQ(buf[2], 5);
}

/* ---- max_opts limit ---- */

static void test_max_opts_limit(void) {
    uint8_t data[] = {NFS_IPOPT_NOP, NFS_IPOPT_NOP, NFS_IPOPT_NOP, NFS_IPOPT_EOL};
    struct nfs_ip_option opts[2];
    size_t nfound;

    ASSERT_EQ(nfs_ip_opts_parse(data, sizeof(data), opts, 2, &nfound), 0);
    ASSERT_EQ(nfound, 2);
}

int main(void) {
    printf("IP Options Tests\n");

    test_parse_eol_only();
    test_parse_nop_stream();
    test_parse_record_route();
    test_build_parse_rr_roundtrip();
    test_build_parse_ts_roundtrip();
    test_pad_alignment();
    test_pad_already_aligned();
    test_pad_5_bytes();
    test_malformed_zero_length();
    test_malformed_length_one();
    test_malformed_length_exceeds();
    test_malformed_truncated();
    test_type_names();
    test_build_rr_invalid();
    test_build_ts_invalid();
    test_parse_null();
    test_parse_mixed();
    test_build_rr_single();
    test_build_ts_single();
    test_max_opts_limit();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
