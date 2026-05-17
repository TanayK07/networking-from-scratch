/* Tests for CIDR and subnetting operations. */

#include "../cidr.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Minimal test harness                                               */
/* ------------------------------------------------------------------ */

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

#define RUN(fn)                                                                                    \
    do {                                                                                           \
        printf("  %-50s ", #fn);                                                                   \
        fn();                                                                                      \
        printf("OK\n");                                                                            \
    } while (0)

/* ------------------------------------------------------------------ */
/*  Tests: IP parsing                                                  */
/* ------------------------------------------------------------------ */

static void test_ip_parse_basic(void) {
    uint32_t ip;
    ASSERT_EQ(nfs_ip_parse("10.0.0.1", &ip), 0);
    ASSERT_EQ(ip, 0x0A000001U);

    ASSERT_EQ(nfs_ip_parse("192.168.1.100", &ip), 0);
    ASSERT_EQ(ip, 0xC0A80164U);

    ASSERT_EQ(nfs_ip_parse("0.0.0.0", &ip), 0);
    ASSERT_EQ(ip, 0x00000000U);

    ASSERT_EQ(nfs_ip_parse("255.255.255.255", &ip), 0);
    ASSERT_EQ(ip, 0xFFFFFFFFU);
}

static void test_ip_parse_errors(void) {
    uint32_t ip;
    ASSERT_EQ(nfs_ip_parse("", &ip), -1);
    ASSERT_EQ(nfs_ip_parse("10.0.0", &ip), -1);
    ASSERT_EQ(nfs_ip_parse("10.0.0.0.1", &ip), -1);
    ASSERT_EQ(nfs_ip_parse("256.0.0.0", &ip), -1);
    ASSERT_EQ(nfs_ip_parse("abc", &ip), -1);
}

static void test_ip_format(void) {
    char buf[16];
    nfs_ip_format(0x0A000001U, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "10.0.0.1") == 0);

    nfs_ip_format(0xC0A80164U, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "192.168.1.100") == 0);

    nfs_ip_format(0x00000000U, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "0.0.0.0") == 0);

    nfs_ip_format(0xFFFFFFFFU, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "255.255.255.255") == 0);
}

/* ------------------------------------------------------------------ */
/*  Tests: CIDR parsing                                                */
/* ------------------------------------------------------------------ */

static void test_cidr_parse_basic(void) {
    struct nfs_cidr c;
    ASSERT_EQ(nfs_cidr_parse("10.0.0.0/24", &c), 0);
    ASSERT_EQ(c.addr, 0x0A000000U);
    ASSERT_EQ(c.prefix_len, 24);

    ASSERT_EQ(nfs_cidr_parse("192.168.1.0/16", &c), 0);
    ASSERT_EQ(c.addr, 0xC0A80100U);
    ASSERT_EQ(c.prefix_len, 16);

    ASSERT_EQ(nfs_cidr_parse("0.0.0.0/0", &c), 0);
    ASSERT_EQ(c.addr, 0x00000000U);
    ASSERT_EQ(c.prefix_len, 0);

    ASSERT_EQ(nfs_cidr_parse("10.1.2.3/32", &c), 0);
    ASSERT_EQ(c.addr, 0x0A010203U);
    ASSERT_EQ(c.prefix_len, 32);
}

static void test_cidr_parse_errors(void) {
    struct nfs_cidr c;
    ASSERT_EQ(nfs_cidr_parse("10.0.0.0", &c), -1);    /* no prefix */
    ASSERT_EQ(nfs_cidr_parse("10.0.0.0/33", &c), -1); /* prefix too large */
    ASSERT_EQ(nfs_cidr_parse("/24", &c), -1);         /* no IP */
    ASSERT_EQ(nfs_cidr_parse("", &c), -1);
}

/* ------------------------------------------------------------------ */
/*  Tests: Mask generation                                             */
/* ------------------------------------------------------------------ */

static void test_mask_generation(void) {
    ASSERT_EQ(nfs_cidr_mask(0), 0x00000000U);
    ASSERT_EQ(nfs_cidr_mask(1), 0x80000000U);
    ASSERT_EQ(nfs_cidr_mask(8), 0xFF000000U);
    ASSERT_EQ(nfs_cidr_mask(16), 0xFFFF0000U);
    ASSERT_EQ(nfs_cidr_mask(24), 0xFFFFFF00U);
    ASSERT_EQ(nfs_cidr_mask(25), 0xFFFFFF80U);
    ASSERT_EQ(nfs_cidr_mask(31), 0xFFFFFFFEU);
    ASSERT_EQ(nfs_cidr_mask(32), 0xFFFFFFFFU);
}

/* ------------------------------------------------------------------ */
/*  Tests: Network and broadcast                                       */
/* ------------------------------------------------------------------ */

static void test_network_address(void) {
    struct nfs_cidr c;
    nfs_cidr_parse("192.168.1.130/24", &c);
    ASSERT_EQ(nfs_cidr_network(&c), 0xC0A80100U); /* 192.168.1.0 */

    nfs_cidr_parse("10.1.2.3/8", &c);
    ASSERT_EQ(nfs_cidr_network(&c), 0x0A000000U); /* 10.0.0.0 */

    nfs_cidr_parse("172.16.5.4/16", &c);
    ASSERT_EQ(nfs_cidr_network(&c), 0xAC100000U); /* 172.16.0.0 */

    nfs_cidr_parse("10.1.2.3/32", &c);
    ASSERT_EQ(nfs_cidr_network(&c), 0x0A010203U); /* 10.1.2.3 */

    nfs_cidr_parse("10.1.2.3/0", &c);
    ASSERT_EQ(nfs_cidr_network(&c), 0x00000000U); /* 0.0.0.0 */
}

static void test_broadcast_address(void) {
    struct nfs_cidr c;
    nfs_cidr_parse("192.168.1.0/24", &c);
    ASSERT_EQ(nfs_cidr_broadcast(&c), 0xC0A801FFU); /* 192.168.1.255 */

    nfs_cidr_parse("10.0.0.0/8", &c);
    ASSERT_EQ(nfs_cidr_broadcast(&c), 0x0AFFFFFFU); /* 10.255.255.255 */

    nfs_cidr_parse("10.1.2.3/32", &c);
    ASSERT_EQ(nfs_cidr_broadcast(&c), 0x0A010203U); /* same as addr */

    nfs_cidr_parse("0.0.0.0/0", &c);
    ASSERT_EQ(nfs_cidr_broadcast(&c), 0xFFFFFFFFU); /* 255.255.255.255 */
}

/* ------------------------------------------------------------------ */
/*  Tests: Host count                                                  */
/* ------------------------------------------------------------------ */

static void test_host_count(void) {
    ASSERT_EQ(nfs_cidr_host_count(24), 254U);
    ASSERT_EQ(nfs_cidr_host_count(16), 65534U);
    ASSERT_EQ(nfs_cidr_host_count(8), 16777214U);
    ASSERT_EQ(nfs_cidr_host_count(32), 1U);
    ASSERT_EQ(nfs_cidr_host_count(31), 2U);
    ASSERT_EQ(nfs_cidr_host_count(30), 2U);
    ASSERT_EQ(nfs_cidr_host_count(25), 126U);
    ASSERT_EQ(nfs_cidr_host_count(0), 4294967294U);
}

/* ------------------------------------------------------------------ */
/*  Tests: Contains                                                    */
/* ------------------------------------------------------------------ */

static void test_contains(void) {
    struct nfs_cidr c;
    nfs_cidr_parse("192.168.1.0/24", &c);

    ASSERT_EQ(nfs_cidr_contains(&c, 0xC0A80101U), 1); /* 192.168.1.1 */
    ASSERT_EQ(nfs_cidr_contains(&c, 0xC0A801FEU), 1); /* 192.168.1.254 */
    ASSERT_EQ(nfs_cidr_contains(&c, 0xC0A80100U), 1); /* 192.168.1.0 (network) */
    ASSERT_EQ(nfs_cidr_contains(&c, 0xC0A801FFU), 1); /* 192.168.1.255 (broadcast) */
    ASSERT_EQ(nfs_cidr_contains(&c, 0xC0A80200U), 0); /* 192.168.2.0 */
    ASSERT_EQ(nfs_cidr_contains(&c, 0x0A000001U), 0); /* 10.0.0.1 */

    /* /0 contains everything */
    nfs_cidr_parse("0.0.0.0/0", &c);
    ASSERT_EQ(nfs_cidr_contains(&c, 0xFFFFFFFFU), 1);
    ASSERT_EQ(nfs_cidr_contains(&c, 0x00000000U), 1);
    ASSERT_EQ(nfs_cidr_contains(&c, 0x08080808U), 1);

    /* /32 contains only the single host */
    nfs_cidr_parse("10.1.2.3/32", &c);
    ASSERT_EQ(nfs_cidr_contains(&c, 0x0A010203U), 1);
    ASSERT_EQ(nfs_cidr_contains(&c, 0x0A010204U), 0);
}

/* ------------------------------------------------------------------ */
/*  Tests: Overlap                                                     */
/* ------------------------------------------------------------------ */

static void test_overlap(void) {
    struct nfs_cidr a, b;

    /* Identical CIDRs overlap */
    nfs_cidr_parse("192.168.1.0/24", &a);
    nfs_cidr_parse("192.168.1.0/24", &b);
    ASSERT_EQ(nfs_cidr_overlap(&a, &b), 1);

    /* Supernet contains subnet */
    nfs_cidr_parse("10.0.0.0/8", &a);
    nfs_cidr_parse("10.1.2.0/24", &b);
    ASSERT_EQ(nfs_cidr_overlap(&a, &b), 1);

    /* Disjoint */
    nfs_cidr_parse("192.168.1.0/24", &a);
    nfs_cidr_parse("192.168.2.0/24", &b);
    ASSERT_EQ(nfs_cidr_overlap(&a, &b), 0);

    /* /0 overlaps with everything */
    nfs_cidr_parse("0.0.0.0/0", &a);
    nfs_cidr_parse("10.0.0.0/8", &b);
    ASSERT_EQ(nfs_cidr_overlap(&a, &b), 1);

    /* Adjacent /25s: overlap check (should not overlap) */
    nfs_cidr_parse("192.168.1.0/25", &a);
    nfs_cidr_parse("192.168.1.128/25", &b);
    ASSERT_EQ(nfs_cidr_overlap(&a, &b), 0);

    /* Partially overlapping: /24 and /25 within it */
    nfs_cidr_parse("192.168.1.0/24", &a);
    nfs_cidr_parse("192.168.1.0/25", &b);
    ASSERT_EQ(nfs_cidr_overlap(&a, &b), 1);
}

/* ------------------------------------------------------------------ */
/*  Tests: Format                                                      */
/* ------------------------------------------------------------------ */

static void test_cidr_format(void) {
    struct nfs_cidr c = {.addr = 0xC0A80100U, .prefix_len = 24};
    char buf[32];
    nfs_cidr_format(&c, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "192.168.1.0/24") == 0);

    c.addr = 0x0A000000U;
    c.prefix_len = 8;
    nfs_cidr_format(&c, buf, sizeof(buf));
    ASSERT_TRUE(strcmp(buf, "10.0.0.0/8") == 0);
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("CIDR and subnetting test suite\n");

    RUN(test_ip_parse_basic);
    RUN(test_ip_parse_errors);
    RUN(test_ip_format);
    RUN(test_cidr_parse_basic);
    RUN(test_cidr_parse_errors);
    RUN(test_mask_generation);
    RUN(test_network_address);
    RUN(test_broadcast_address);
    RUN(test_host_count);
    RUN(test_contains);
    RUN(test_overlap);
    RUN(test_cidr_format);

    printf("\n%d/%d assertions passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        printf("FAIL\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
