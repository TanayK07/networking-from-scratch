/* Unit tests for NDP (Neighbor Discovery Protocol). */

#include "../ndp.h"
#include <arpa/inet.h>
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

static const uint8_t TEST_TARGET[16] = {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

static const uint8_t TEST_MAC[6] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};

/* ---- NS tests ---- */

static void test_ns_build_basic(void) {
    uint8_t buf[128];
    int n = nfs_ndp_build_ns(TEST_TARGET, NULL, buf, sizeof(buf));
    ASSERT_EQ(n, 24); /* Just the NS header, no options */
    ASSERT_EQ(buf[0], NFS_NDP_NS);
    ASSERT_EQ(buf[1], 0); /* code */
}

static void test_ns_build_with_mac(void) {
    uint8_t buf[128];
    int n = nfs_ndp_build_ns(TEST_TARGET, TEST_MAC, buf, sizeof(buf));
    ASSERT_EQ(n, 32); /* 24 + 8 (SrcLLA option) */
    /* Check option type and length */
    ASSERT_EQ(buf[24], NFS_NDP_OPT_SRC_LLA);
    ASSERT_EQ(buf[25], 1); /* 1 unit of 8 bytes */
    ASSERT_TRUE(memcmp(buf + 26, TEST_MAC, 6) == 0);
}

static void test_ns_parse(void) {
    uint8_t buf[128];
    int n = nfs_ndp_build_ns(TEST_TARGET, TEST_MAC, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    struct nfs_ndp_ns ns;
    ASSERT_EQ(nfs_ndp_parse_ns(buf, (size_t)n, &ns), 0);
    ASSERT_EQ(ns.type, NFS_NDP_NS);
    ASSERT_EQ(ns.code, 0);
    ASSERT_TRUE(memcmp(ns.target, TEST_TARGET, 16) == 0);
}

static void test_ns_build_null_target(void) {
    uint8_t buf[128];
    ASSERT_EQ(nfs_ndp_build_ns(NULL, NULL, buf, sizeof(buf)), -1);
}

static void test_ns_build_buffer_too_small(void) {
    uint8_t buf[10];
    ASSERT_EQ(nfs_ndp_build_ns(TEST_TARGET, NULL, buf, sizeof(buf)), -1);
}

/* ---- NA tests ---- */

static void test_na_build_basic(void) {
    uint8_t buf[128];
    int n = nfs_ndp_build_na(TEST_TARGET, NFS_NDP_NA_FLAG_S | NFS_NDP_NA_FLAG_O, NULL, buf,
                             sizeof(buf));
    ASSERT_EQ(n, 24);
    ASSERT_EQ(buf[0], NFS_NDP_NA);
}

static void test_na_build_with_mac(void) {
    uint8_t buf[128];
    int n = nfs_ndp_build_na(TEST_TARGET, NFS_NDP_NA_FLAG_R, TEST_MAC, buf, sizeof(buf));
    ASSERT_EQ(n, 32);
    ASSERT_EQ(buf[24], NFS_NDP_OPT_TGT_LLA);
}

static void test_na_parse_flags(void) {
    uint8_t buf[128];
    uint32_t flags = NFS_NDP_NA_FLAG_R | NFS_NDP_NA_FLAG_S | NFS_NDP_NA_FLAG_O;
    int n = nfs_ndp_build_na(TEST_TARGET, flags, NULL, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    struct nfs_ndp_na na;
    ASSERT_EQ(nfs_ndp_parse_na(buf, (size_t)n, &na), 0);
    ASSERT_TRUE(na.flags & NFS_NDP_NA_FLAG_R);
    ASSERT_TRUE(na.flags & NFS_NDP_NA_FLAG_S);
    ASSERT_TRUE(na.flags & NFS_NDP_NA_FLAG_O);
    ASSERT_TRUE(memcmp(na.target, TEST_TARGET, 16) == 0);
}

static void test_na_parse_no_flags(void) {
    uint8_t buf[128];
    int n = nfs_ndp_build_na(TEST_TARGET, 0, NULL, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    struct nfs_ndp_na na;
    ASSERT_EQ(nfs_ndp_parse_na(buf, (size_t)n, &na), 0);
    ASSERT_EQ(na.flags, 0u);
}

/* ---- RS tests ---- */

static void test_rs_build_basic(void) {
    uint8_t buf[64];
    int n = nfs_ndp_build_rs(NULL, buf, sizeof(buf));
    ASSERT_EQ(n, 8);
    ASSERT_EQ(buf[0], NFS_NDP_RS);
}

static void test_rs_build_with_mac(void) {
    uint8_t buf[64];
    int n = nfs_ndp_build_rs(TEST_MAC, buf, sizeof(buf));
    ASSERT_EQ(n, 16); /* 8 + 8 */
}

static void test_rs_parse(void) {
    uint8_t buf[64];
    int n = nfs_ndp_build_rs(NULL, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    struct nfs_ndp_rs rs;
    ASSERT_EQ(nfs_ndp_parse_rs(buf, (size_t)n, &rs), 0);
    ASSERT_EQ(rs.type, NFS_NDP_RS);
}

/* ---- RA tests ---- */

static void test_ra_build_basic(void) {
    uint8_t buf[128];
    int n = nfs_ndp_build_ra(64, NFS_NDP_RA_FLAG_M, 1800, 30000, 1000, NULL, buf, sizeof(buf));
    ASSERT_EQ(n, 16);
    ASSERT_EQ(buf[0], NFS_NDP_RA);
}

static void test_ra_parse(void) {
    uint8_t buf[128];
    int n = nfs_ndp_build_ra(64, NFS_NDP_RA_FLAG_M | NFS_NDP_RA_FLAG_O, 1800, 30000, 1000, TEST_MAC,
                             buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    struct nfs_ndp_ra ra;
    ASSERT_EQ(nfs_ndp_parse_ra(buf, (size_t)n, &ra), 0);
    ASSERT_EQ(ra.cur_hop_limit, 64);
    ASSERT_TRUE(ra.flags & NFS_NDP_RA_FLAG_M);
    ASSERT_TRUE(ra.flags & NFS_NDP_RA_FLAG_O);
    ASSERT_EQ(ra.router_lifetime, 1800);
    ASSERT_EQ(ra.reachable_time, 30000u);
    ASSERT_EQ(ra.retrans_timer, 1000u);
}

/* ---- Parse type tests ---- */

static void test_parse_type_ns(void) {
    uint8_t buf[128];
    int n = nfs_ndp_build_ns(TEST_TARGET, NULL, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(nfs_ndp_parse_type(buf, (size_t)n), NFS_NDP_NS);
}

static void test_parse_type_na(void) {
    uint8_t buf[128];
    int n = nfs_ndp_build_na(TEST_TARGET, 0, NULL, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(nfs_ndp_parse_type(buf, (size_t)n), NFS_NDP_NA);
}

static void test_parse_type_too_short(void) {
    uint8_t buf[] = {NFS_NDP_NS, 0, 0};
    ASSERT_EQ(nfs_ndp_parse_type(buf, 3), -1);
}

static void test_parse_type_invalid(void) {
    uint8_t buf[] = {0x01, 0x00, 0x00, 0x00}; /* type 1 = not NDP */
    ASSERT_EQ(nfs_ndp_parse_type(buf, 4), -1);
}

static void test_parse_type_bad_code(void) {
    uint8_t buf[24];
    nfs_ndp_build_ns(TEST_TARGET, NULL, buf, sizeof(buf));
    buf[1] = 1; /* invalid code */
    ASSERT_EQ(nfs_ndp_parse_type(buf, 24), -1);
}

/* ---- Option iteration tests ---- */

static void test_option_iteration(void) {
    uint8_t buf[64];
    int n = nfs_ndp_build_ns(TEST_TARGET, TEST_MAC, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    /* Options start after NS header (24 bytes) */
    const uint8_t *opts = buf + 24;
    size_t opt_len = (size_t)n - 24;

    size_t offset = 0;
    struct nfs_ndp_parsed_option opt;
    ASSERT_EQ(nfs_ndp_option_next(opts, opt_len, &offset, &opt), 0);
    ASSERT_EQ(opt.type, NFS_NDP_OPT_SRC_LLA);
    ASSERT_EQ(opt.total_len, 8);
    ASSERT_EQ(opt.data_len, 6);
    ASSERT_TRUE(memcmp(opt.data, TEST_MAC, 6) == 0);

    /* No more options */
    ASSERT_EQ(nfs_ndp_option_next(opts, opt_len, &offset, &opt), -1);
}

static void test_option_zero_length(void) {
    uint8_t bad_opt[] = {1, 0, 0, 0, 0, 0, 0, 0};
    size_t offset = 0;
    struct nfs_ndp_parsed_option opt;
    ASSERT_EQ(nfs_ndp_option_next(bad_opt, sizeof(bad_opt), &offset, &opt), -1);
}

/* ---- Format tests ---- */

static void test_format_ipv6(void) {
    char str[64];
    nfs_ndp_format_ipv6(TEST_TARGET, str, sizeof(str));
    ASSERT_TRUE(strcmp(str, "fe80:0000:0000:0000:0000:0000:0000:0001") == 0);
}

static void test_format_ipv6_null(void) {
    char str[64];
    nfs_ndp_format_ipv6(NULL, str, sizeof(str));
    ASSERT_EQ(str[0], '\0');
}

/* ---- Error handling tests ---- */

static void test_parse_null(void) {
    struct nfs_ndp_ns ns;
    ASSERT_EQ(nfs_ndp_parse_ns(NULL, 24, &ns), -1);
    ASSERT_EQ(nfs_ndp_parse_ns((const uint8_t *)"x", 24, NULL), -1);
}

static void test_parse_wrong_type(void) {
    uint8_t buf[128];
    nfs_ndp_build_na(TEST_TARGET, 0, NULL, buf, sizeof(buf));
    struct nfs_ndp_ns ns;
    /* Try to parse NA as NS */
    ASSERT_EQ(nfs_ndp_parse_ns(buf, 24, &ns), -1);
}

int main(void) {
    printf("NDP Neighbor Discovery Tests\n");

    test_ns_build_basic();
    test_ns_build_with_mac();
    test_ns_parse();
    test_ns_build_null_target();
    test_ns_build_buffer_too_small();
    test_na_build_basic();
    test_na_build_with_mac();
    test_na_parse_flags();
    test_na_parse_no_flags();
    test_rs_build_basic();
    test_rs_build_with_mac();
    test_rs_parse();
    test_ra_build_basic();
    test_ra_parse();
    test_parse_type_ns();
    test_parse_type_na();
    test_parse_type_too_short();
    test_parse_type_invalid();
    test_parse_type_bad_code();
    test_option_iteration();
    test_option_zero_length();
    test_format_ipv6();
    test_format_ipv6_null();
    test_parse_null();
    test_parse_wrong_type();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
